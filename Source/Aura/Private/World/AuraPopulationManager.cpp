// Copyright Druid Mechanics

#include "World/AuraPopulationManager.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Character/AuraCivilian.h"
#include "Engine/World.h"
#include "Game/AuraGameModeBase.h"
#include "World/AuraCivilianSpawnVolume.h"
#include "World/AuraPopulationSpawnDefinition.h"

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
	bDefinitionsInitialized = true;
	++InitializationGeneration;
	UE_LOG(LogAura, Display, TEXT("[Population][Init] Published generation=%d rows=%d workProfiles=%d before StartPlay."),
		InitializationGeneration, PopulationRows.Num(), WorkProfiles.Num());
	return true;
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

void UAuraPopulationManager::ScheduleInitialPopulation()
{
	if (!bDefinitionsInitialized || bInitialPopulationFinalized || bFinalizeScheduled || !GetWorld())
	{
		return;
	}

	bFinalizeScheduled = true;
	FTimerDelegate Delegate;
	Delegate.BindWeakLambda(this, [this]()
	{
		bFinalizeScheduled = false;
		FinalizeInitialPopulation();
	});
	GetWorld()->GetTimerManager().SetTimerForNextTick(Delegate);
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

	bInitialPopulationFinalized = true;
	const FName CurrentMapId = FindCurrentMapId();
	if (CurrentMapId.IsNone())
	{
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
	return bSuccess;
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

AAuraCivilianSpawnVolume* UAuraPopulationManager::FindVolumeForRow(const FAuraPopulationSpawnRow& Row, int32 AttemptIndex) const
{
	if (Row.SpawnVolumeIds.Num() == 0) return nullptr;
	for (int32 Offset = 0; Offset < Row.SpawnVolumeIds.Num(); ++Offset)
	{
		const int32 Index = (AttemptIndex + Offset) % Row.SpawnVolumeIds.Num();
		if (const TObjectPtr<AAuraCivilianSpawnVolume>* Volume = RegisteredVolumes.Find(Row.SpawnVolumeIds[Index]))
		{
			if (IsValid(Volume->Get())) return Volume->Get();
		}
	}
	return nullptr;
}

bool UAuraPopulationManager::SpawnInitialMember(const FAuraPopulationSpawnRow& Row, int32 SlotIndex)
{
	const FName MemberId = BuildDeterministicMemberId(Row.PopulationId, SlotIndex);
	if (MemberId.IsNone() || ReservedMemberIds.Contains(MemberId) || LiveMembers.Contains(MemberId)) return false;

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
		AAuraCivilianSpawnVolume* Volume = FindVolumeForRow(Row, Attempt);
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
			LiveMembers.Add(MemberId, Civilian);
			ActorToMember.Add(Civilian, MemberId);
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
		LiveMembers.Remove(*MemberId);
		UE_LOG(LogAura, Display, TEXT("[Population][Lifecycle] member=%s actor=%s destroyed; slot remains reserved until a future lifecycle day."),
			*MemberId->ToString(), *GetNameSafe(DestroyedActor));
		ActorToMember.Remove(DestroyedActor);
	}
}

const AAuraCivilian* UAuraPopulationManager::FindLiveMember(FName PopulationMemberId) const
{
	const TWeakObjectPtr<AAuraCivilian>* Member = LiveMembers.Find(PopulationMemberId);
	return Member ? Member->Get() : nullptr;
}
