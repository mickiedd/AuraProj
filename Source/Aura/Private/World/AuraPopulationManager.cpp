// Copyright Druid Mechanics

#include "World/AuraPopulationManager.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Combat/AuraCombatTypes.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "AI/AuraCivilianWorkProfileRegistry.h"
#include "Battle/AuraBattleZoneConfig.h"
#include "Battle/AuraBattleDirector.h"
#include "Character/AuraCivilian.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Engine/World.h"
#include "Game/AuraGameModeBase.h"
#include "World/AuraCivilianSpawnVolume.h"
#include "World/AuraCivilianActivityMarker.h"
#include "World/AuraPopulationSpawnDefinition.h"
#include "TimerManager.h"

FName UAuraPopulationManager::BuildDeterministicMemberId(FName PopulationId, int32 SlotIndex)
{
	if (PopulationId.IsNone() || SlotIndex < 0)
	{
		return NAME_None;
	}
	return FName(*FString::Printf(TEXT("%s:%d"), *PopulationId.ToString(), SlotIndex));
}

bool UAuraPopulationManager::IsValidCivilianActorClass(const UClass* ActorClass)
{
	return ActorClass && ActorClass->IsChildOf(AAuraCivilian::StaticClass());
}

bool UAuraPopulationManager::InitializeDefinitions(FString& OutError)
{
	if (bDefinitionsInitialized)
	{
		return true;
	}

	Definition = NewObject<UAuraPopulationSpawnDefinition>(this);
	if (!Definition || !Definition->LoadDefinitions(OutError))
	{
		UE_LOG(LogAura, Error, TEXT("[Population][Init] Definitions rejected: %s"), *OutError);
		return false;
	}

	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	if (!RoleInfo)
	{
		OutError = TEXT("Authoritative role registry is unavailable.");
		UE_LOG(LogAura, Error, TEXT("[Population][Init] %s"), *OutError);
		return false;
	}

	for (const FAuraPopulationSpawnRow& Row : Definition->GetPopulationRows())
	{
		FString RowError;
		if (!ValidateRoleAndClass(Row, RowError))
		{
			OutError += OutError.IsEmpty() ? FString() : TEXT("\n");
			OutError += RowError;
			continue;
		}
		PopulationRows.Add(Row);
	}
	if (!OutError.IsEmpty())
	{
		UE_LOG(LogAura, Error, TEXT("[Population][Init] Unsafe population rows rejected:\n%s"), *OutError);
		PopulationRows.Empty();
		return false;
	}

	WorkProfiles = Definition->GetWorkProfiles();
	WorkProfileRegistry = NewObject<UAuraCivilianWorkProfileRegistry>(this, TEXT("WorkProfileRegistry"));
	if (!WorkProfileRegistry)
	{
		OutError = TEXT("Unable to create the immutable Civilian work-profile registry.");
		PopulationRows.Empty();
		return false;
	}
	FString WorkProfileError;
	if (!WorkProfileRegistry->Initialize(WorkProfiles, WorkProfileError))
	{
		OutError = WorkProfileError;
		PopulationRows.Empty();
		return false;
	}
	for (const FAuraPopulationSpawnRow& Row : PopulationRows)
	{
		for (int32 SlotIndex = 0; SlotIndex < Row.MaximumCount; ++SlotIndex)
		{
			FAuraPopulationMemberState MemberState;
			if (!ResolveMemberState(Row, SlotIndex, MemberState))
			{
				OutError += FString::Printf(TEXT("Unable to resolve population slot %s:%d.\n"), *Row.PopulationId.ToString(), SlotIndex);
				continue;
			}
			FAuraPopulationRuntimeSlot& Slot = RuntimeSlots.Add(MemberState.PopulationMemberId);
			Slot.Member = MemberState;
			Slot.Generation = InitializationGeneration + 1;
			Slot.LastTransitionReason = TEXT("DefinitionLoaded");
		}
	}
	if (!OutError.IsEmpty())
	{
		RuntimeSlots.Reset();
		return false;
	}
	bDefinitionsInitialized = true;
	++InitializationGeneration;
	UE_LOG(LogAura, Display, TEXT("[Population][Init] Published generation=%d rows=%d workProfiles=%d before StartPlay."),
		InitializationGeneration, PopulationRows.Num(), WorkProfiles.Num());
	return true;
}

const FAuraCivilianWorkProfile* UAuraPopulationManager::FindWorkProfile(FName WorkProfileId) const
{
	return WorkProfileRegistry ? WorkProfileRegistry->Find(WorkProfileId) : nullptr;
}

void UAuraPopulationManager::GetLiveMembers(TArray<AAuraCivilian*>& OutMembers) const
{
	OutMembers.Reset();
	for (const TPair<FName, TWeakObjectPtr<AAuraCivilian>>& Pair : LiveMembers)
	{
		if (AAuraCivilian* Civilian = Pair.Value.Get())
		{
			OutMembers.Add(Civilian);
		}
	}
	OutMembers.Sort([](const AAuraCivilian& A, const AAuraCivilian& B)
	{
		return A.GetPopulationMemberState().PopulationMemberId.LexicalLess(B.GetPopulationMemberState().PopulationMemberId);
	});
}

bool UAuraPopulationManager::ValidateBattleZoneRegistrations(const UAuraBattleZoneConfig* ZoneConfig, FString& OutError) const
{
	OutError.Empty();
	if (!bDefinitionsInitialized || !ZoneConfig)
	{
		OutError = TEXT("Population definitions or battle-zone configuration are unavailable.");
		return false;
	}

	const auto HasZone = [ZoneConfig](const FName MapId, const FName ZoneId)
	{
		for (const FAuraBattleZoneDefinition& Zone : ZoneConfig->GetZones())
		{
			if (Zone.MapId == MapId && Zone.ZoneId == ZoneId) return true;
		}
		return false;
	};

	const FName CurrentMapId = FindCurrentMapId();
	for (const FAuraPopulationSpawnRow& Row : PopulationRows)
	{
		if (!HasZone(Row.MapId, Row.ZoneId))
		{
			OutError += FString::Printf(TEXT("population '%s' references unknown battle zone '%s' on map '%s'.\n"),
				*Row.PopulationId.ToString(), *Row.ZoneId.ToString(), *Row.MapId.ToString());
		}
		if (Row.bSpawnOnLoad && Row.MapId == CurrentMapId)
		{
			for (const FName VolumeId : Row.SpawnVolumeIds)
			{
				const TObjectPtr<AAuraCivilianSpawnVolume>* Volume = RegisteredVolumes.Find(VolumeId);
				if (!Volume || !IsValid(Volume->Get()))
				{
					OutError += FString::Printf(TEXT("population '%s' requires unregistered spawn volume '%s'.\n"),
						*Row.PopulationId.ToString(), *VolumeId.ToString());
				}
			}
		}
	}

	for (const TPair<FName, TObjectPtr<AAuraCivilianActivityMarker>>& Pair : RegisteredActivityMarkers)
	{
		const AAuraCivilianActivityMarker* Marker = Pair.Value;
		if (Marker && !HasZone(CurrentMapId, Marker->GetZoneId()))
		{
			OutError += FString::Printf(TEXT("activity marker '%s' references unknown battle zone '%s' on map '%s'.\n"),
				*Pair.Key.ToString(), *Marker->GetZoneId().ToString(), *CurrentMapId.ToString());
		}
	}

	OutError.TrimEndInline();
	return OutError.IsEmpty();
}

void UAuraPopulationManager::RegisterActivityMarker(AAuraCivilianActivityMarker* Marker)
{
	if (!Marker || !bDefinitionsInitialized || Marker->GetMarkerId().IsNone()) return;
	if (const TObjectPtr<AAuraCivilianActivityMarker>* Existing = RegisteredActivityMarkers.Find(Marker->GetMarkerId()))
	{
		if (Existing->Get() != Marker)
		{
			UE_LOG(LogAura, Error, TEXT("[CivilianAI][Marker] Duplicate marker id=%s actor=%s."),
				*Marker->GetMarkerId().ToString(), *GetNameSafe(Marker));
		}
		return;
	}
	RegisteredActivityMarkers.Add(Marker->GetMarkerId(), Marker);
	UE_LOG(LogAura, Display, TEXT("[CivilianAI][Marker] Registered id=%s zone=%s actor=%s capacity=%d."),
		*Marker->GetMarkerId().ToString(), *Marker->GetZoneId().ToString(), *GetNameSafe(Marker), Marker->GetCapacity());
}

void UAuraPopulationManager::UnregisterActivityMarker(AAuraCivilianActivityMarker* Marker)
{
	if (!Marker) return;
	RegisteredActivityMarkers.Remove(Marker->GetMarkerId());
	ActivityMarkerReservations.Remove(Marker->GetMarkerId());
}

AAuraCivilianActivityMarker* UAuraPopulationManager::FindActivityMarker(FName MarkerId) const
{
	const TObjectPtr<AAuraCivilianActivityMarker>* Marker = RegisteredActivityMarkers.Find(MarkerId);
	return Marker ? Marker->Get() : nullptr;
}

void UAuraPopulationManager::GetActivityMarkers(TArray<AAuraCivilianActivityMarker*>& OutMarkers) const
{
	OutMarkers.Reset();
	for (const TPair<FName, TObjectPtr<AAuraCivilianActivityMarker>>& Pair : RegisteredActivityMarkers)
	{
		if (AAuraCivilianActivityMarker* Marker = Pair.Value.Get()) OutMarkers.Add(Marker);
	}
	OutMarkers.Sort([](const AAuraCivilianActivityMarker& A, const AAuraCivilianActivityMarker& B)
	{
		return A.GetMarkerId().LexicalLess(B.GetMarkerId());
	});
}

bool UAuraPopulationManager::TryReserveActivityMarker(FName MarkerId, FName PopulationMemberId)
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && (!GetWorld() || !GetWorld()->GetAuthGameMode())) return false;
	AAuraCivilianActivityMarker* Marker = FindActivityMarker(MarkerId);
	if (!Marker || PopulationMemberId.IsNone() || !Marker->IsEnabled()) return false;
	TSet<FName>& Reservations = ActivityMarkerReservations.FindOrAdd(MarkerId);
	if (Reservations.Contains(PopulationMemberId)) return true;
	if (Reservations.Num() >= FMath::Max(1, Marker->GetCapacity())) return false;
	Reservations.Add(PopulationMemberId);
	UE_LOG(LogAura, Verbose, TEXT("[CivilianAI][Marker] Reserved marker=%s member=%s count=%d."),
		*MarkerId.ToString(), *PopulationMemberId.ToString(), Reservations.Num());
	return true;
}

void UAuraPopulationManager::ReleaseActivityMarkerReservation(FName MarkerId, FName PopulationMemberId)
{
	if (TSet<FName>* Reservations = ActivityMarkerReservations.Find(MarkerId))
	{
		Reservations->Remove(PopulationMemberId);
		if (Reservations->IsEmpty()) ActivityMarkerReservations.Remove(MarkerId);
	}
}

void UAuraPopulationManager::ReleaseAllActivityMarkerReservations(FName PopulationMemberId)
{
	for (auto It = ActivityMarkerReservations.CreateIterator(); It; ++It)
	{
		It.Value().Remove(PopulationMemberId);
		if (It.Value().IsEmpty()) It.RemoveCurrent();
	}
}

void UAuraPopulationManager::RegisterSpawnVolume(AAuraCivilianSpawnVolume* Volume)
{
	if (!Volume || !bDefinitionsInitialized || Volume->GetSpawnVolumeId().IsNone())
	{
		return;
	}

	const FName VolumeId = Volume->GetSpawnVolumeId();
	if (const TObjectPtr<AAuraCivilianSpawnVolume>* Existing = RegisteredVolumes.Find(VolumeId))
	{
		if (Existing->Get() != Volume)
		{
			UE_LOG(LogAura, Error, TEXT("[Population][Volume] Duplicate spawn volume id '%s'; rejecting actor=%s."),
				*VolumeId.ToString(), *GetNameSafe(Volume));
		}
		return;
	}
	RegisteredVolumes.Add(VolumeId, Volume);
	UE_LOG(LogAura, Display, TEXT("[Population][Volume] Registered id=%s actor=%s."), *VolumeId.ToString(), *GetNameSafe(Volume));
}

void UAuraPopulationManager::UnregisterSpawnVolume(AAuraCivilianSpawnVolume* Volume)
{
	if (!Volume) return;
	const FName VolumeId = Volume->GetSpawnVolumeId();
	if (const TObjectPtr<AAuraCivilianSpawnVolume>* Existing = RegisteredVolumes.Find(VolumeId))
	{
		if (Existing->Get() == Volume) RegisteredVolumes.Remove(VolumeId);
	}
}

bool UAuraPopulationManager::FinalizeInitialPopulation()
{
	if (bInitialPopulationFinalized)
	{
		return true;
	}
	if (!bDefinitionsInitialized || !GetWorld() || !GetWorld()->GetAuthGameMode())
	{
		return false;
	}

	if (AAuraGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AAuraGameModeBase>())
	{
		BoundBattleDirector = GameMode->GetBattleDirectorMutable();
		if (AAuraBattleDirector* Director = BoundBattleDirector.Get(); Director && !PhaseChangedDelegateHandle.IsValid())
		{
			PhaseChangedDelegateHandle = Director->OnPhaseChanged.AddUObject(this, &UAuraPopulationManager::HandleBattlePhaseChanged);
		}
	}
	const FName CurrentMapId = FindCurrentMapId();
	if (CurrentMapId.IsNone())
	{
		bInitialPopulationFinalized = true;
		UE_LOG(LogAura, Display, TEXT("[Population][Finalize] Current map is not a configured population map; no rows spawned."));
		return true;
	}

	bool bSuccess = true;
	for (const FAuraPopulationSpawnRow& Row : PopulationRows)
	{
		if (!Row.bSpawnOnLoad || Row.MapId != CurrentMapId) continue;
		int32 SpawnedCount = 0;
		for (int32 SlotIndex = 0; SlotIndex < Row.MaximumCount && SpawnedCount < Row.InitialCount; ++SlotIndex)
		{
			if (SpawnInitialMember(Row, SlotIndex)) ++SpawnedCount;
		}
		if (SpawnedCount != Row.InitialCount)
		{
			bSuccess = false;
			UE_LOG(LogAura, Error, TEXT("[Population][Finalize] Failed to reach initialCount=%d for population=%s; spawned=%d."),
				Row.InitialCount, *Row.PopulationId.ToString(), SpawnedCount);
		}
	}
	UE_LOG(LogAura, Display, TEXT("[Population][Finalize] generation=%d map=%s liveMembers=%d success=%d."),
		InitializationGeneration, *CurrentMapId.ToString(), LiveMembers.Num(), bSuccess ? 1 : 0);
	if (!bSuccess)
	{
		RollBackInitialPopulation();
		return false;
	}
	bInitialPopulationFinalized = true;
	return bSuccess;
}

void UAuraPopulationManager::RollBackInitialPopulation()
{
	TArray<TWeakObjectPtr<AAuraCivilian>> SpawnedActors;
	LiveMembers.GenerateValueArray(SpawnedActors);
	for (const TWeakObjectPtr<AAuraCivilian>& Actor : SpawnedActors)
	{
		if (AAuraCivilian* Civilian = Actor.Get())
		{
			Civilian->OnDestroyed.RemoveDynamic(this, &UAuraPopulationManager::OnMemberDestroyed);
			Civilian->Destroy();
		}
	}
	LiveMembers.Reset();
	ActorToMember.Reset();
	ReservedMemberIds.Reset();
	ActivityMarkerReservations.Reset();
	bInitialPopulationFinalized = false;
	UE_LOG(LogAura, Error, TEXT("[Population][Finalize] Rolled back partial initial population."));
}

bool UAuraPopulationManager::IsCurrentMapRow(const FAuraPopulationSpawnRow& Row) const
{
	return Row.MapId == FindCurrentMapId();
}

FName UAuraPopulationManager::FindCurrentMapId() const
{
	if (!GetWorld() || !GetWorld()->PersistentLevel || !Definition)
	{
		return NAME_None;
	}

	FString CurrentPackage = GetWorld()->PersistentLevel->GetOutermost()->GetName();
	CurrentPackage.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
	for (const TPair<FName, FString>& Pair : Definition->GetLevelMapPaths())
	{
		if (CurrentPackage.Equals(Pair.Value, ESearchCase::IgnoreCase) || CurrentPackage.EndsWith(Pair.Value, ESearchCase::IgnoreCase))
		{
			return Pair.Key;
		}
	}

	const FString CurrentLevelName = GetWorld()->GetMapName();
	for (const TPair<FName, FString>& Pair : Definition->GetLevelMapPaths())
	{
		if (CurrentLevelName.Equals(Pair.Key.ToString(), ESearchCase::IgnoreCase)) return Pair.Key;
	}
	return NAME_None;
}

bool UAuraPopulationManager::ValidateRoleAndClass(const FAuraPopulationSpawnRow& Row, FString& OutError) const
{
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	const FRoleDefaultInfo* Role = RoleInfo ? RoleInfo->RoleInformation.Find(Row.RoleId) : nullptr;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	if (!Role)
	{
		OutError = FString::Printf(TEXT("population '%s' references unknown role '%s'"), *Row.PopulationId.ToString(), *Row.RoleId.ToString());
		return false;
	}
	if (!Role->EntityType.MatchesTagExact(Tags.Entity_AmbientNPC)
		|| !Role->ControlType.MatchesTagExact(Tags.Control_CivilianAI)
		|| !Role->Faction.MatchesTagExact(Tags.Faction_Civilian)
		|| !Role->CombatProfile.MatchesTagExact(Tags.Combat_Civilian)
		|| !Role->DeathPolicy.MatchesTagExact(Tags.Death_PopulationRespawn)
		|| !Role->bTargetable || !Role->bCanBeDamaged || Role->bAllowFriendlyFire
		|| Role->bCanAttack || !Role->StartupAbilities.IsEmpty() || !Role->StartupPassiveAbilities.IsEmpty()
		|| !Role->UnlockableAbilities.IsEmpty() || !Role->StartupAbilityDefinitionPaths.IsEmpty()
		|| !Role->StartupPassiveAbilityDefinitionPaths.IsEmpty() || !Role->DefaultLMBAbilityDefinitionPath.IsEmpty()
		|| Role->DefaultLMBAbility != nullptr)
	{
		OutError = FString::Printf(TEXT("population '%s' role '%s' is not a safe empty Civilian role"), *Row.PopulationId.ToString(), *Row.RoleId.ToString());
		return false;
	}

	const UClass* ActorClass = LoadClass<AAuraCivilian>(nullptr, *Row.ActorClassPath);
	if (!IsValidCivilianActorClass(ActorClass))
	{
		OutError = FString::Printf(TEXT("population '%s' actorClass '%s' is not loadable as AAuraCivilian"), *Row.PopulationId.ToString(), *Row.ActorClassPath);
		return false;
	}
	return true;
}

bool UAuraPopulationManager::ResolveMemberState(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, FAuraPopulationMemberState& OutState) const
{
	OutState = FAuraPopulationMemberState();
	OutState.PopulationId = Row.PopulationId;
	OutState.PopulationSlotIndex = SlotIndex;
	OutState.PopulationMemberId = BuildDeterministicMemberId(Row.PopulationId, SlotIndex);
	OutState.WorkProfileId = Row.DefaultWorkProfileId;
	OutState.ZoneId = Row.ZoneId;

	for (const FAuraPopulationMemberOverride& Override : Row.MemberOverrides)
	{
		if (Override.SlotIndex == SlotIndex)
		{
			if (!Override.WorkProfileId.IsNone()) OutState.WorkProfileId = Override.WorkProfileId;
			OutState.MerchantDefinitionId = Override.MerchantDefinitionId;
			break;
		}
	}
	return OutState.IsValid() && Definition && Definition->FindWorkProfile(OutState.WorkProfileId) != nullptr;
}

AAuraCivilianSpawnVolume* UAuraPopulationManager::FindVolumeForRow(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, int32 AttemptIndex) const
{
	if (Row.SpawnVolumeIds.Num() == 0) return nullptr;
	for (int32 Offset = 0; Offset < Row.SpawnVolumeIds.Num(); ++Offset)
	{
		// Start each deterministic member in a different village volume. This keeps
		// large world populations distributed while preserving bounded retries when
		// a particular village has no valid nav/collision-free candidate.
		const int32 Index = (SlotIndex + AttemptIndex + Offset) % Row.SpawnVolumeIds.Num();
		if (const TObjectPtr<AAuraCivilianSpawnVolume>* Volume = RegisteredVolumes.Find(Row.SpawnVolumeIds[Index]))
		{
			if (IsValid(Volume->Get())) return Volume->Get();
		}
	}
	return nullptr;
}

bool UAuraPopulationManager::SpawnInitialMember(const FAuraPopulationSpawnRow& Row, int32 SlotIndex)
{
	return SpawnMember(Row, SlotIndex, TEXT("InitialSpawn"));
}

bool UAuraPopulationManager::SpawnMember(const FAuraPopulationSpawnRow& Row, int32 SlotIndex, const TCHAR* Reason)
{
	const FName MemberId = BuildDeterministicMemberId(Row.PopulationId, SlotIndex);
	FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberId);
	if (bShuttingDown || MemberId.IsNone() || !Slot || ReservedMemberIds.Contains(MemberId) || LiveMembers.Contains(MemberId)) return false;

	FAuraPopulationMemberState MemberState;
	if (!ResolveMemberState(Row, SlotIndex, MemberState)) return false;
	ReservedMemberIds.Add(MemberId);

	UWorld* World = GetWorld();
	const UClass* LoadedClass = LoadClass<AAuraCivilian>(nullptr, *Row.ActorClassPath);
	TSubclassOf<AAuraCivilian> CivilianClass = const_cast<UClass*>(LoadedClass);
	if (!World || !CivilianClass)
	{
		ReservedMemberIds.Remove(MemberId);
		return false;
	}

	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		AAuraCivilianSpawnVolume* Volume = FindVolumeForRow(Row, SlotIndex, Attempt);
		FTransform SpawnTransform;
		if (!Volume || !Volume->GetCandidateTransform(Attempt, SpawnTransform)) continue;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AAuraCivilian* Civilian = World->SpawnActorDeferred<AAuraCivilian>(CivilianClass, SpawnTransform, nullptr, nullptr, SpawnParameters.SpawnCollisionHandlingOverride);
		if (!Civilian) continue;

		Civilian->SetRequestedCivilianRoleId(Row.RoleId);
		Civilian->SetPopulationMemberState(MemberState);
		Civilian->OnDestroyed.AddDynamic(this, &UAuraPopulationManager::OnMemberDestroyed);
		Civilian->FinishSpawning(SpawnTransform);
		Civilian->AlignToGroundForSpawn();

		const UAuraAttributeSet* Attributes = Cast<UAuraAttributeSet>(Civilian->GetAttributeSet());
		const bool bValidRole = Civilian->GetAppliedRoleState().RoleId == Row.RoleId
			&& Civilian->GetAppliedRoleState().IsValid()
			&& Civilian->HasValidCombatIdentity()
			&& Civilian->GetPopulationMemberState() == MemberState
			&& Attributes && Attributes->GetHealth() > 0.f && Attributes->GetMaxHealth() > 0.f;
		if (bValidRole)
		{
			ReservedMemberIds.Remove(MemberId);
			LiveMembers.Add(MemberId, Civilian);
			ActorToMember.Add(Civilian, MemberId);
			Slot->Actor = Civilian;
			Slot->DeathSequence = 0;
			Slot->RetryCount = 0;
			BindMemberLifeState(*Slot, Civilian);
			TransitionSlot(*Slot, EAuraPopulationSlotState::Active, Reason);
			UE_LOG(LogAura, Display, TEXT("[Population][Spawn] population=%s slot=%d member=%s actor=%s role=%s."),
				*Row.PopulationId.ToString(), SlotIndex, *MemberId.ToString(), *GetNameSafe(Civilian), *Row.RoleId.ToString());
			return true;
		}

		UE_LOG(LogAura, Error, TEXT("[Population][Spawn] Rejected actor=%s member=%s after deferred initialization; role=%s applied=%s identity=%d."),
			*GetNameSafe(Civilian), *MemberId.ToString(), *Row.RoleId.ToString(), *Civilian->GetAppliedRoleState().RoleId.ToString(), Civilian->HasValidCombatIdentity() ? 1 : 0);
		Civilian->Destroy();
	}

	ReservedMemberIds.Remove(MemberId);
	return false;
}

void UAuraPopulationManager::OnMemberDestroyed(AActor* DestroyedActor)
{
	if (const FName* MemberId = ActorToMember.Find(DestroyedActor))
	{
		const FName MemberIdValue = *MemberId;
		LiveMembers.Remove(MemberIdValue);
		ActorToMember.Remove(DestroyedActor);
		ReleaseAllActivityMarkerReservations(MemberIdValue);
		if (FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberIdValue))
		{
			UnbindMemberLifeState(*Slot);
			Slot->Actor.Reset();
			if (!bShuttingDown && Slot->State == EAuraPopulationSlotState::Active)
			{
				TransitionSlot(*Slot, EAuraPopulationSlotState::Suppressed, TEXT("UnexpectedActorDestruction"));
			}
			UE_LOG(LogAura, Display, TEXT("[Population][Lifecycle] member=%s actor=%s destroyed state=%d; destruction did not infer death or schedule refill."),
				*MemberIdValue.ToString(), *GetNameSafe(DestroyedActor), static_cast<int32>(Slot->State));
		}
	}
}

void UAuraPopulationManager::HandlePopulationDeath(const FAuraDeathEvent& Event)
{
	if (bShuttingDown || !Event.IsValid() || !Event.DeathPolicyTag.MatchesTagExact(FAuraGameplayTags::Get().Death_PopulationRespawn)) return;
	AAuraCivilian* Civilian = Cast<AAuraCivilian>(Event.VictimActor);
	if (!Civilian) return;
	const FAuraPopulationMemberState MemberState = Civilian->GetPopulationMemberState();
	if (MemberState.PopulationMemberId.IsNone()) return;
	FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberState.PopulationMemberId);
	if (!Slot || Slot->Generation != InitializationGeneration || Slot->Actor.Get() != Civilian
		|| Slot->State != EAuraPopulationSlotState::Active || Event.DeathSequence <= Slot->DeathSequence) return;
	++RecordedPopulationDeathCount;
	Slot->DeathSequence = Event.DeathSequence;
	TransitionSlot(*Slot, EAuraPopulationSlotState::Dying, TEXT("AuthoritativeDeath"));
	LiveMembers.Remove(MemberState.PopulationMemberId);
	ReleaseAllActivityMarkerReservations(MemberState.PopulationMemberId);
	UE_LOG(LogAura, Display, TEXT("[Population][Lifecycle] Recorded authoritative death member=%s sequence=%d exactly once."),
		*MemberState.PopulationMemberId.ToString(), Event.DeathSequence);
	if (const UAuraCombatStateComponent* State = Civilian->GetCombatStateComponent(); State && State->GetLifeState() == EAuraCombatLifeState::Dead)
	{
		HandleMemberLifeStateChanged(MemberState.PopulationMemberId, Slot->Generation, EAuraCombatLifeState::Dead);
	}
}

const FAuraPopulationSpawnRow* UAuraPopulationManager::FindRow(FName PopulationId) const
{
	return PopulationRows.FindByPredicate([PopulationId](const FAuraPopulationSpawnRow& Row) { return Row.PopulationId == PopulationId; });
}

void UAuraPopulationManager::TransitionSlot(FAuraPopulationRuntimeSlot& Slot, EAuraPopulationSlotState NewState, const TCHAR* Reason)
{
	Slot.State = NewState;
	Slot.TransitionServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Slot.LastTransitionReason = Reason ? Reason : TEXT("Unknown");
	UE_LOG(LogAura, Display, TEXT("[Population][Transition] member=%s generation=%d state=%d reason=%s."),
		*Slot.Member.PopulationMemberId.ToString(), Slot.Generation, static_cast<int32>(Slot.State), *Slot.LastTransitionReason);
}

void UAuraPopulationManager::BindMemberLifeState(FAuraPopulationRuntimeSlot& Slot, AAuraCivilian* Civilian)
{
	UnbindMemberLifeState(Slot);
	if (!Civilian || !Civilian->GetCombatStateComponentMutable()) return;
	const FName MemberId = Slot.Member.PopulationMemberId;
	const int32 Generation = Slot.Generation;
	Slot.LifeStateDelegateHandle = Civilian->GetCombatStateComponentMutable()->OnLifeStateChanged.AddWeakLambda(this,
		[this, MemberId, Generation](EAuraCombatLifeState State) { HandleMemberLifeStateChanged(MemberId, Generation, State); });
}

void UAuraPopulationManager::UnbindMemberLifeState(FAuraPopulationRuntimeSlot& Slot)
{
	if (Slot.LifeStateDelegateHandle.IsValid())
	{
		if (AAuraCivilian* Civilian = Slot.Actor.Get(); Civilian && Civilian->GetCombatStateComponentMutable())
		{
			Civilian->GetCombatStateComponentMutable()->OnLifeStateChanged.Remove(Slot.LifeStateDelegateHandle);
		}
		Slot.LifeStateDelegateHandle.Reset();
	}
}

void UAuraPopulationManager::HandleMemberLifeStateChanged(FName MemberId, int32 ExpectedGeneration, EAuraCombatLifeState NewState)
{
	FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberId);
	if (bShuttingDown || !Slot || Slot->Generation != ExpectedGeneration || Slot->State != EAuraPopulationSlotState::Dying
		|| NewState != EAuraCombatLifeState::Dead) return;
	const FAuraPopulationSpawnRow* Row = FindRow(Slot->Member.PopulationId);
	if (!Row) return;
	TransitionSlot(*Slot, EAuraPopulationSlotState::Corpse, TEXT("AuthoritativeDeadState"));
	ScheduleCorpseCleanup(*Slot, *Row);
}

void UAuraPopulationManager::ScheduleCorpseCleanup(FAuraPopulationRuntimeSlot& Slot, const FAuraPopulationSpawnRow& Row)
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(Slot.CorpseTimer)) return;
	const FName MemberId = Slot.Member.PopulationMemberId;
	const int32 Generation = Slot.Generation;
	const int32 DeathSequence = Slot.DeathSequence;
	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(this, [this, MemberId, Generation, DeathSequence]()
	{
		ExecuteCorpseCleanup(MemberId, Generation, DeathSequence);
	});
	GetWorld()->GetTimerManager().SetTimer(Slot.CorpseTimer, Delegate, FMath::Max(Row.RespawnPolicy.CorpseSeconds, KINDA_SMALL_NUMBER), false);
}

void UAuraPopulationManager::ExecuteCorpseCleanup(FName MemberId, int32 ExpectedGeneration, int32 ExpectedDeathSequence)
{
	FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberId);
	if (bShuttingDown || !Slot || Slot->Generation != ExpectedGeneration || Slot->DeathSequence != ExpectedDeathSequence
		|| Slot->State != EAuraPopulationSlotState::Corpse) return;
	const FAuraPopulationSpawnRow* Row = FindRow(Slot->Member.PopulationId);
	if (!Row) return;
	if (AAuraCivilian* Corpse = Slot->Actor.Get())
	{
		Corpse->SetActorEnableCollision(false);
		Corpse->SetActorHiddenInGame(true);
		UnbindMemberLifeState(*Slot);
		ActorToMember.Remove(Corpse);
		Corpse->OnDestroyed.RemoveDynamic(this, &UAuraPopulationManager::OnMemberDestroyed);
		Corpse->Destroy();
	}
	Slot->Actor.Reset();
	Slot->CorpseTimer.Invalidate();
	if (!Row->RespawnPolicy.bEnabled || !IsRefillAllowed(*Row))
	{
		TransitionSlot(*Slot, EAuraPopulationSlotState::Suppressed, Row->RespawnPolicy.bEnabled ? TEXT("RefillPolicySuppressed") : TEXT("RespawnDisabled"));
		return;
	}
	ScheduleRefill(*Slot, *Row, Row->RespawnPolicy.DelaySeconds, TEXT("CorpseCleaned"));
}

bool UAuraPopulationManager::IsRefillAllowed(const FAuraPopulationSpawnRow& Row) const
{
	const AAuraBattleDirector* Director = BoundBattleDirector.Get();
	if (!Director || !Director->IsAuthorityConfigurationReady()) return false;
	const UEnum* PhaseEnum = StaticEnum<EAuraBattlePhase>();
	const FName PhaseName = PhaseEnum ? FName(*PhaseEnum->GetNameStringByValue(static_cast<int64>(Director->GetCurrentPhase()))) : NAME_None;
	return Row.RespawnPolicy.AllowedBattlePhases.IsEmpty() || Row.RespawnPolicy.AllowedBattlePhases.Contains(PhaseName);
}

void UAuraPopulationManager::ScheduleRefill(FAuraPopulationRuntimeSlot& Slot, const FAuraPopulationSpawnRow& Row, float DelaySeconds, const TCHAR* Reason)
{
	if (bShuttingDown || !GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(Slot.RefillTimer)) return;
	if (!IsRefillAllowed(Row))
	{
		TransitionSlot(Slot, EAuraPopulationSlotState::Suppressed, TEXT("PhaseDisallowedBeforeSchedule"));
		return;
	}
	TransitionSlot(Slot, EAuraPopulationSlotState::RefillPending, Reason);
	const FName MemberId = Slot.Member.PopulationMemberId;
	const int32 Generation = Slot.Generation;
	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(this, [this, MemberId, Generation]() { ExecuteRefill(MemberId, Generation); });
	GetWorld()->GetTimerManager().SetTimer(Slot.RefillTimer, Delegate, FMath::Max(DelaySeconds, KINDA_SMALL_NUMBER), false);
}

void UAuraPopulationManager::ExecuteRefill(FName MemberId, int32 ExpectedGeneration)
{
	FAuraPopulationRuntimeSlot* Slot = RuntimeSlots.Find(MemberId);
	if (bShuttingDown || !Slot || Slot->Generation != ExpectedGeneration || Slot->State != EAuraPopulationSlotState::RefillPending) return;
	Slot->RefillTimer.Invalidate();
	const FAuraPopulationSpawnRow* Row = FindRow(Slot->Member.PopulationId);
	if (!Row || !IsRefillAllowed(*Row))
	{
		TransitionSlot(*Slot, EAuraPopulationSlotState::Suppressed, TEXT("PhaseDisallowedAtExecution"));
		return;
	}
	if (SpawnMember(*Row, Slot->Member.PopulationSlotIndex, TEXT("DeterministicRefill"))) return;
	if (++Slot->RetryCount <= 3)
	{
		ScheduleRefill(*Slot, *Row, FMath::Max(1.f, Row->RespawnPolicy.DelaySeconds), TEXT("BoundedSpawnRetry"));
	}
	else
	{
		TransitionSlot(*Slot, EAuraPopulationSlotState::Suppressed, TEXT("RetryLimitReached"));
	}
}

void UAuraPopulationManager::HandleBattlePhaseChanged(EAuraBattlePhase NewPhase)
{
	if (bShuttingDown || !GetWorld()) return;
	for (TPair<FName, FAuraPopulationRuntimeSlot>& Pair : RuntimeSlots)
	{
		FAuraPopulationRuntimeSlot& Slot = Pair.Value;
		const FAuraPopulationSpawnRow* Row = FindRow(Slot.Member.PopulationId);
		if (!Row || !Row->RespawnPolicy.bEnabled) continue;
		if (Slot.State == EAuraPopulationSlotState::RefillPending && !IsRefillAllowed(*Row))
		{
			GetWorld()->GetTimerManager().ClearTimer(Slot.RefillTimer);
			TransitionSlot(Slot, EAuraPopulationSlotState::Suppressed, TEXT("PhaseChangedDisallowed"));
		}
		else if (Slot.State == EAuraPopulationSlotState::Suppressed && !Slot.Actor.IsValid() && IsRefillAllowed(*Row))
		{
			ScheduleRefill(Slot, *Row, Row->RespawnPolicy.DelaySeconds, TEXT("PhaseChangedAllowed"));
		}
	}
}

int32 UAuraPopulationManager::GetTrackedCorpseTimerCount() const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld()) for (const TPair<FName, FAuraPopulationRuntimeSlot>& Pair : RuntimeSlots)
	{
		if (World->GetTimerManager().IsTimerActive(Pair.Value.CorpseTimer)) ++Count;
	}
	return Count;
}

int32 UAuraPopulationManager::GetTrackedRefillTimerCount() const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld()) for (const TPair<FName, FAuraPopulationRuntimeSlot>& Pair : RuntimeSlots)
	{
		if (World->GetTimerManager().IsTimerActive(Pair.Value.RefillTimer)) ++Count;
	}
	return Count;
}

FAuraPopulationDebugSnapshot UAuraPopulationManager::BuildDebugSnapshot() const
{
	FAuraPopulationDebugSnapshot Snapshot;
	Snapshot.SchemaVersion = Definition ? Definition->GetSchemaVersion() : 0;
	Snapshot.ManagerGeneration = InitializationGeneration;
	Snapshot.MapId = FindCurrentMapId();
	if (const AAuraBattleDirector* Director = BoundBattleDirector.Get())
	{
		const UEnum* PhaseEnum = StaticEnum<EAuraBattlePhase>();
		Snapshot.DirectorPhase = PhaseEnum ? FName(*PhaseEnum->GetNameStringByValue(static_cast<int64>(Director->GetCurrentPhase()))) : NAME_None;
		Snapshot.BattleEventId = Director->GetActiveBattleEventId();
	}
	for (const FAuraPopulationSpawnRow& Row : PopulationRows) if (Row.MapId == Snapshot.MapId) Snapshot.MaximumCount += Row.MaximumCount;
	for (const TPair<FName, FAuraPopulationRuntimeSlot>& Pair : RuntimeSlots)
	{
		const FAuraPopulationRuntimeSlot& Slot = Pair.Value;
		if (Slot.Member.PopulationId.IsNone()) continue;
		FAuraPopulationSlotSnapshot& Item = Snapshot.Slots.AddDefaulted_GetRef();
		Item.PopulationId = Slot.Member.PopulationId;
		Item.SlotIndex = Slot.Member.PopulationSlotIndex;
		Item.PopulationMemberId = Slot.Member.PopulationMemberId;
		Item.State = Slot.State;
		Item.Generation = Slot.Generation;
		Item.ActorNetworkPath = Slot.Actor.IsValid() ? Slot.Actor->GetPathName() : FString();
		Item.TransitionServerTime = Slot.TransitionServerTime;
		Item.LastTransitionReason = Slot.LastTransitionReason;
		Item.DeathSequence = Slot.DeathSequence;
		if (const UWorld* World = GetWorld())
		{
			Item.RemainingCorpseDelay = FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(Slot.CorpseTimer));
			Item.RemainingRefillDelay = FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(Slot.RefillTimer));
		}
		switch (Slot.State)
		{
		case EAuraPopulationSlotState::Active: ++Snapshot.ActiveCount; break;
		case EAuraPopulationSlotState::Corpse: ++Snapshot.CorpseCount; break;
		case EAuraPopulationSlotState::RefillPending: ++Snapshot.PendingCount; break;
		case EAuraPopulationSlotState::Suppressed: ++Snapshot.SuppressedCount; break;
		default: break;
		}
	}
	Snapshot.Slots.Sort([](const FAuraPopulationSlotSnapshot& A, const FAuraPopulationSlotSnapshot& B) { return A.PopulationMemberId.LexicalLess(B.PopulationMemberId); });
	return Snapshot;
}

void UAuraPopulationManager::Shutdown()
{
	if (bShuttingDown) return;
	bShuttingDown = true;
	++InitializationGeneration;
	if (AAuraBattleDirector* Director = BoundBattleDirector.Get(); Director && PhaseChangedDelegateHandle.IsValid())
	{
		Director->OnPhaseChanged.Remove(PhaseChangedDelegateHandle);
	}
	PhaseChangedDelegateHandle.Reset();
	if (UWorld* World = GetWorld()) for (TPair<FName, FAuraPopulationRuntimeSlot>& Pair : RuntimeSlots)
	{
		World->GetTimerManager().ClearTimer(Pair.Value.CorpseTimer);
		World->GetTimerManager().ClearTimer(Pair.Value.RefillTimer);
		UnbindMemberLifeState(Pair.Value);
		if (AAuraCivilian* Civilian = Pair.Value.Actor.Get()) Civilian->OnDestroyed.RemoveDynamic(this, &UAuraPopulationManager::OnMemberDestroyed);
	}
	ActivityMarkerReservations.Reset();
	RecordedPopulationDeathCount = 0;
	ReservedMemberIds.Reset();
	ActorToMember.Reset();
	LiveMembers.Reset();
	RuntimeSlots.Reset();
	BoundBattleDirector.Reset();
}

const AAuraCivilian* UAuraPopulationManager::FindLiveMember(FName PopulationMemberId) const
{
	const TWeakObjectPtr<AAuraCivilian>* Member = LiveMembers.Find(PopulationMemberId);
	return Member ? Member->Get() : nullptr;
}
