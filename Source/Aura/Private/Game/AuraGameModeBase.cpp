// Copyright Druid Mechanics


#include "Game/AuraGameModeBase.h"

#include "EngineUtils.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Game/AuraGameInstance.h"
#include "Game/AuraPersistenceSubsystem.h"
#include "Tests/AuraRoleBattleNetworkProbe.h"
#include "Game/LoadScreenSaveGame.h"
#include "GameFramework/PlayerStart.h"
#include "Interaction/SaveInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Game/ServerTravelComponent.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UI/ViewModel/MVVM_LoadSlot.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NavigationSystem.h"
#include "Player/AuraPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/NetConnection.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Actor/LevelJumpPortal.h"
#include "Character/AuraEnemy.h"
#include "Character/AuraCivilian.h"
#include "AI/AuraCivilianAIController.h"
#include "World/AuraPopulationManager.h"
#include "Combat/AuraDeathPolicyDispatcher.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraInventoryComponent.h"
#include "Economy/AuraCommerceSubsystem.h"
#include "Economy/AuraMerchantComponent.h"
#include "Battle/AuraBattleDirector.h"
#include "World/AuraCivilianSpawnVolume.h"
#include "World/AuraCivilianWorkMarker.h"
#include "World/AuraCivilianObservationMarker.h"
#include "World/AuraCivilianShelterMarker.h"
#include "Character/AuraCharacter.h"
#include "AI/BTService_FindNearestHostile.h"
#include "Battle/AuraBattleZoneConfig.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "IPAddress.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace AuraGameModeBaseInternal
{
	static FString JsonEscape(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Escaped;
	}

	static FString GetGsmServerAuthToken()
	{
		FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("AURA_GSM_SERVER_AUTH_TOKEN"));
		if (Token.IsEmpty())
		{
			Token = FPlatformMisc::GetEnvironmentVariable(TEXT("AURA_GSM_AUTH_TOKEN"));
		}
		Token.TrimStartAndEndInline();
		return Token;
	}

	static FString GetGsmReadyNonce()
	{
		FString Nonce = FPlatformMisc::GetEnvironmentVariable(TEXT("AURA_GSM_SERVER_READY_NONCE"));
		Nonce.TrimStartAndEndInline();
		return Nonce;
	}
}

void AAuraGameModeBase::SaveSlotData(UMVVM_LoadSlot* LoadSlot, int32 SlotIndex)
{
	if (UGameplayStatics::DoesSaveGameExist(LoadSlot->GetLoadSlotName(), SlotIndex))
	{
		UGameplayStatics::DeleteGameInSlot(LoadSlot->GetLoadSlotName(), SlotIndex);
	}
	USaveGame* SaveGameObject = UGameplayStatics::CreateSaveGameObject(LoadScreenSaveGameClass);
	ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveGameObject);
	LoadScreenSaveGame->PlayerName = LoadSlot->GetPlayerName();
	LoadScreenSaveGame->SaveSlotStatus = Taken;
	LoadScreenSaveGame->MapName = LoadSlot->GetMapName();
	LoadScreenSaveGame->MapAssetName = LoadSlot->MapAssetName;
	LoadScreenSaveGame->PlayerStartTag = LoadSlot->PlayerStartTag;
	LoadScreenSaveGame->Role = LoadSlot->GetRole();
	UE_LOG(LogAura, Log, TEXT("[Role][SaveSlot] Slot='%s' Index=%d Role='%s' PlayerName='%s'."),
		*LoadSlot->GetLoadSlotName(), SlotIndex, *LoadScreenSaveGame->Role.ToString(), *LoadScreenSaveGame->PlayerName);

	UGameplayStatics::SaveGameToSlot(LoadScreenSaveGame, LoadSlot->GetLoadSlotName(), SlotIndex);
}

ULoadScreenSaveGame* AAuraGameModeBase::GetSaveSlotData(const FString& SlotName, int32 SlotIndex) const
{
	USaveGame* SaveGameObject = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		SaveGameObject = UGameplayStatics::LoadGameFromSlot(SlotName, SlotIndex);
	}
	else
	{
		SaveGameObject = UGameplayStatics::CreateSaveGameObject(LoadScreenSaveGameClass);
	}
	ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveGameObject);
	return LoadScreenSaveGame;
}

void AAuraGameModeBase::DeleteSlot(const FString& SlotName, int32 SlotIndex)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, SlotIndex);
	}
}

ULoadScreenSaveGame* AAuraGameModeBase::RetrieveInGameSaveData()
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());

	const FString InGameLoadSlotName = AuraGameInstance->LoadSlotName;
	const int32 InGameLoadSlotIndex = AuraGameInstance->LoadSlotIndex;

	return GetSaveSlotData(InGameLoadSlotName, InGameLoadSlotIndex);
}

void AAuraGameModeBase::SaveInGameProgressData(ULoadScreenSaveGame* SaveObject)
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());

	const FString InGameLoadSlotName = AuraGameInstance->LoadSlotName;
	const int32 InGameLoadSlotIndex = AuraGameInstance->LoadSlotIndex;
	AuraGameInstance->PlayerStartTag = SaveObject->PlayerStartTag;

	UGameplayStatics::SaveGameToSlot(SaveObject, InGameLoadSlotName, InGameLoadSlotIndex);
}

void AAuraGameModeBase::SaveWorldState(UWorld* World, const FString& DestinationMapAssetName) const
{
	FString WorldName = World->GetMapName();
	WorldName.RemoveFromStart(World->StreamingLevelsPrefix);

	UAuraGameInstance* AuraGI = Cast<UAuraGameInstance>(GetGameInstance());
	check(AuraGI);

	if (ULoadScreenSaveGame* SaveGame = GetSaveSlotData(AuraGI->LoadSlotName, AuraGI->LoadSlotIndex))
	{
		if (DestinationMapAssetName != FString(""))
		{
			SaveGame->MapAssetName = DestinationMapAssetName;
			SaveGame->MapName = GetMapNameFromMapAssetName(DestinationMapAssetName);
		}
		
		if (!SaveGame->HasMap(WorldName))
		{
			FSavedMap NewSavedMap;
			NewSavedMap.MapAssetName = WorldName;
			SaveGame->SavedMaps.Add(NewSavedMap);
		}

		FSavedMap SavedMap = SaveGame->GetSavedMapWithMapName(WorldName);
		SavedMap.SavedActors.Empty(); // clear it out, we'll fill it in with "actors"

		for (FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;

			if (!IsValid(Actor) || !Actor->Implements<USaveInterface>()) continue;

			FSavedActor SavedActor;
			SavedActor.ActorName = Actor->GetFName();
			SavedActor.Transform = Actor->GetTransform();

			FMemoryWriter MemoryWriter(SavedActor.Bytes);

			FObjectAndNameAsStringProxyArchive Archive(MemoryWriter, true);
			Archive.ArIsSaveGame = true;

			Actor->Serialize(Archive);

			SavedMap.SavedActors.AddUnique(SavedActor);
		}

		for (FSavedMap& MapToReplace : SaveGame->SavedMaps)
		{
			if (MapToReplace.MapAssetName == WorldName)
			{
				MapToReplace = SavedMap;
			}
		}
		UGameplayStatics::SaveGameToSlot(SaveGame, AuraGI->LoadSlotName, AuraGI->LoadSlotIndex);
	}
}

void AAuraGameModeBase::LoadWorldState(UWorld* World) const
{
	FString WorldName = World->GetMapName();
	WorldName.RemoveFromStart(World->StreamingLevelsPrefix);

	UAuraGameInstance* AuraGI = Cast<UAuraGameInstance>(GetGameInstance());
	check(AuraGI);

	if (UGameplayStatics::DoesSaveGameExist(AuraGI->LoadSlotName, AuraGI->LoadSlotIndex))
	{

		ULoadScreenSaveGame* SaveGame = Cast<ULoadScreenSaveGame>(UGameplayStatics::LoadGameFromSlot(AuraGI->LoadSlotName, AuraGI->LoadSlotIndex));
		if (SaveGame == nullptr)
		{
			UE_LOG(LogAura, Error, TEXT("Failed to load slot"));
			return;
		}
		
		for (FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;

			if (!Actor->Implements<USaveInterface>()) continue;

			for (FSavedActor SavedActor : SaveGame->GetSavedMapWithMapName(WorldName).SavedActors)
			{
				if (SavedActor.ActorName == Actor->GetFName())
				{
					if (ISaveInterface::Execute_ShouldLoadTransform(Actor))
					{
						Actor->SetActorTransform(SavedActor.Transform);
					}

					FMemoryReader MemoryReader(SavedActor.Bytes);

					FObjectAndNameAsStringProxyArchive Archive(MemoryReader, true);
					Archive.ArIsSaveGame = true;
					Actor->Serialize(Archive); // converts binary bytes back into variables

					ISaveInterface::Execute_LoadActor(Actor);
				}
			}
		}
	}

}

void AAuraGameModeBase::TravelToMap(UMVVM_LoadSlot* Slot)
{
	const FString SlotName = Slot->GetLoadSlotName();
	const int32 SlotIndex = Slot->SlotIndex;

	UServerTravelComponent::RouteToMapBySoftPtrViaLoadingLevel(this, Maps.FindChecked(Slot->GetMapName()));
}

FString AAuraGameModeBase::GetMapNameFromMapAssetName(const FString& MapAssetName) const
{
	for (auto& Map : Maps)
	{
		if (Map.Value.ToSoftObjectPath().GetAssetName() == MapAssetName)
		{
			return Map.Key;
		}
	}
	return FString();
}

void AAuraGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	WorldReadiness = EAuraWorldReadiness::Initializing;
	WorldReadinessReason = TEXT("Role/Battle startup is initializing.");
	bEconomyRegistryLoadedForCurrentWorld = false;
	const FAuraRoleLoadResult LoadResult = UAuraAbilitySystemLibrary::LoadRoleInfoCandidate(this);
	if (LoadResult.bCanPublish && LoadResult.Candidate)
	{
		RoleInfo = LoadResult.Candidate;
		UE_LOG(LogAura, Display, TEXT("[RoleConfig][InitGame] Published version=%d roles=%d default=%s before PreLogin."),
			LoadResult.PublishedVersion, RoleInfo->RoleInformation.Num(), *RoleInfo->DefaultRole.ToString());
		if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay5ConfigProbe")))
		{
			FString Error;
			const bool bAuraAccepted = UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, TEXT("Aura"), Error);
			const bool bBungeeAccepted = UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, TEXT("BungeeMan"), Error);
			const bool bCivilianRejected = !UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, TEXT("Civilian"), Error);
			const bool bUnknownRejected = !UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, TEXT("Unknown"), Error);
			UE_LOG(LogAura, Display, TEXT("[Day5ConfigProbe][Server] ValidStartup=1 AuraAccepted=%d BungeeAccepted=%d CivilianRejected=%d UnknownRejected=%d SavedDefaultValidation=%d"),
				bAuraAccepted, bBungeeAccepted, bCivilianRejected, bUnknownRejected,
				RoleInfo->IsPlayerRoleSelectable(RoleInfo->DefaultRole));
		}
	}
	else
	{
		RoleInfo = nullptr;
		UE_LOG(LogAura, Error, TEXT("[RoleConfig][InitGame] Role service unavailable; startup candidate rejected: %s"), *LoadResult.ToLogString());
		if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay5ConfigProbe")))
		{
			UE_LOG(LogAura, Display, TEXT("[Day5ConfigProbe][Server] InvalidStartupRejected=1 RoleServiceUnavailable=1"));
		}
	}

	PopulationManager = NewObject<UAuraPopulationManager>(this, TEXT("PopulationManager"));
	if (PopulationManager)
	{
		FString PopulationError;
		if (!PopulationManager->InitializeDefinitions(PopulationError))
		{
			UE_LOG(LogAura, Error, TEXT("[Population][InitGame] Population definitions unavailable; no civilians will spawn: %s"), *PopulationError);
		}
	}
	EconomyRegistry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAuraEconomyRegistrySubsystem>() : nullptr;
	PersistenceSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAuraPersistenceSubsystem>() : nullptr;
	if (PersistenceSubsystem)
	{
		FString PersistenceError;
		if (!PersistenceSubsystem->ConfigureAuthorityWorld(MapName, PersistenceError,
			PopulationManager ? PopulationManager->GetCurrentMapId() : NAME_None))
		{
			UE_LOG(LogAura, Error, TEXT("[Persistence][InitGame] Authority persistence configuration rejected: %s"), *PersistenceError);
		}
	}
	if (EconomyRegistry && PopulationManager && PopulationManager->IsInitialized())
	{
		FString EconomyError;
		bEconomyRegistryLoadedForCurrentWorld = EconomyRegistry->LoadAndPublishAuthorityRegistry(this, PopulationManager->GetPopulationRows(), EconomyError);
		if (!bEconomyRegistryLoadedForCurrentWorld)
		{
			UE_LOG(LogAura, Error, TEXT("[Economy][InitGame] Registry candidate rejected atomically: %s"), *EconomyError);
		}
	}
	DeathPolicyDispatcher = NewObject<UAuraDeathPolicyDispatcher>(this, TEXT("DeathPolicyDispatcher"));
	if (DeathPolicyDispatcher)
	{
		DeathPolicyDispatcher->Initialize(this);
		DeathPolicyDispatcher->OnAuthoritativeDeath.AddUObject(this, &AAuraGameModeBase::HandleAuthoritativeDeath);
	}
}

void AAuraGameModeBase::HandleAuthoritativeDeath(const FAuraDeathEvent& Event)
{
	if (!HasAuthority()) return;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (Event.DeathPolicyTag.MatchesTagExact(GameplayTags.Death_PlayerRespawn))
	{
		if (AAuraCharacter* Player = Cast<AAuraCharacter>(Event.VictimActor))
		{
			PlayerDied(Player, Player->DeathTime);
		}
	}
	else if (Event.DeathPolicyTag.MatchesTagExact(GameplayTags.Death_EnemyLoot))
	{
		if (AAuraEnemy* Enemy = Cast<AAuraEnemy>(Event.VictimActor))
		{
			Enemy->ApplyEnemyDeathPolicy(Event);
		}
	}
	else if (Event.DeathPolicyTag.MatchesTagExact(GameplayTags.Death_PopulationRespawn))
	{
		if (PopulationManager)
		{
			PopulationManager->HandlePopulationDeath(Event);
		}
	}
}

void AAuraGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	if (GetNetMode() != NM_Standalone && WorldReadiness != EAuraWorldReadiness::Ready)
	{
		if (WorldReadiness == EAuraWorldReadiness::Unhealthy)
		{
			ErrorMessage = FString::Printf(TEXT("World startup is unhealthy: %s"), *WorldReadinessReason);
		}
		else
		{
			ErrorMessage = FString::Printf(TEXT("World startup is still initializing: %s"), *WorldReadinessReason);
		}
		UE_LOG(LogAura, Warning, TEXT("[WorldReadiness][PreLogin] Rejected address=%s reason=%s"), *Address, *ErrorMessage);
		return;
	}

	const FName RequestedRole(*UGameplayStatics::ParseOption(Options, TEXT("Role")));
	if (!UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, RequestedRole, ErrorMessage))
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleLogin][PreLogin] Rejected address=%s role=%s reason=%s"), *Address, *RequestedRole.ToString(), *ErrorMessage);
		return;
	}
	if (PersistenceSubsystem && PersistenceSubsystem->IsPersistentLoginRequired())
	{
		FAuraPlayerProfileId ProfileIdentity;
		if (!PersistenceSubsystem->ResolveConnectionIdentity(UniqueId, Options, ProfileIdentity, ErrorMessage))
		{
			UE_LOG(LogAura, Warning, TEXT("[Persistence][PreLogin] Rejected address=%s reason=%s"), *Address, *ErrorMessage);
			return;
		}
	}
	UE_LOG(LogAura, Display, TEXT("[RoleLogin][PreLogin] Accepted address=%s role=%s"), *Address, *RequestedRole.ToString());
}

FString AAuraGameModeBase::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const bool bListenHostPlayer = GetNetMode() == NM_ListenServer && IsValid(NewPlayerController) && NewPlayerController->IsLocalController();
	if (GetNetMode() != NM_Standalone
		&& !(bListenHostPlayer && WorldReadiness == EAuraWorldReadiness::Initializing)
		&& WorldReadiness != EAuraWorldReadiness::Ready)
	{
		return FString::Printf(TEXT("World startup is not ready: %s"), *WorldReadinessReason);
	}
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	if (!IsValid(NewPlayerController))
	{
		return Result;
	}

	FString RequestedName = UGameplayStatics::ParseOption(Options, TEXT("PlayerName"));
	if (RequestedName.IsEmpty())
	{
		RequestedName = UGameplayStatics::ParseOption(Options, TEXT("Name"));
	}

	const FString DisambiguationToken = BuildConnectionDisambiguationToken(NewPlayerController, UniqueId);
	const FString UniqueName = BuildUniquePlayerName(RequestedName, DisambiguationToken, NewPlayerController->PlayerState);
	ChangeName(NewPlayerController, UniqueName, false);
	if (GetNetMode() == NM_Standalone)
	{
		return Result; // Login/loading map local player is not a server connection request.
	}

	const FName RequestedRole(*UGameplayStatics::ParseOption(Options, TEXT("Role")));
	FString RoleError;
	AAuraPlayerState* AuraPlayerState = NewPlayerController->GetPlayerState<AAuraPlayerState>();
	if (!IsValid(AuraPlayerState) || !UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(RoleInfo, RequestedRole, RoleError))
	{
		UE_LOG(LogAura, Error, TEXT("[RoleLogin][InitNewPlayer] Accepted option could not be revalidated for controller=%s role=%s: %s"),
			*GetNameSafe(NewPlayerController), *RequestedRole.ToString(), *RoleError);
		return RoleError.IsEmpty() ? TEXT("Connection role state is unavailable.") : RoleError;
	}
	if (PersistenceSubsystem && PersistenceSubsystem->IsPersistentLoginRequired())
	{
		FString PersistenceError;
		if (!PersistenceSubsystem->PreparePlayerProfile(NewPlayerController, UniqueId, Options, RequestedRole, PersistenceError))
		{
			UE_LOG(LogAura, Warning, TEXT("[Persistence][InitNewPlayer] Rejected profile preparation: %s"), *PersistenceError);
			return PersistenceError.IsEmpty() ? TEXT("Persistent profile could not be prepared.") : PersistenceError;
		}
	}
	AuraPlayerState->SetPendingAcceptedRoleId(RequestedRole);
	UE_LOG(LogAura, Display, TEXT("[RoleLogin][InitNewPlayer] PendingAcceptedRoleId=%s playerState=%s"), *RequestedRole.ToString(), *GetNameSafe(AuraPlayerState));

	UE_LOG(LogTemp, Display, TEXT("Assigned player name '%s' (requested: '%s', token: '%s')"), *UniqueName, *RequestedName, *DisambiguationToken);

	return Result;
}

void AAuraGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (GetNetMode() == NM_Standalone)
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}
	if (!(GetNetMode() == NM_ListenServer && IsValid(NewPlayer) && NewPlayer->IsLocalController()
		&& WorldReadiness == EAuraWorldReadiness::Initializing)
		&& WorldReadiness != EAuraWorldReadiness::Ready)
	{
		UE_LOG(LogAura, Warning, TEXT("[WorldReadiness] Blocked HandleStartingNewPlayer while startup is not ready: %s"), *WorldReadinessReason);
		return;
	}
	const AAuraPlayerState* AuraPlayerState = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (!IsValid(AuraPlayerState) || !AuraPlayerState->HasPendingAcceptedRoleId())
	{
		UE_LOG(LogAura, Error, TEXT("[RoleLogin] Blocked HandleStartingNewPlayer without a connection-scoped accepted role for %s."), *GetNameSafe(NewPlayer));
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AAuraGameModeBase::RestartPlayer(AController* NewPlayer)
{
	if (GetNetMode() == NM_Standalone)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}
	if (!(GetNetMode() == NM_ListenServer && IsValid(NewPlayer) && NewPlayer->IsLocalController()
		&& WorldReadiness == EAuraWorldReadiness::Initializing)
		&& WorldReadiness != EAuraWorldReadiness::Ready)
	{
		UE_LOG(LogAura, Warning, TEXT("[WorldReadiness] Blocked RestartPlayer while startup is not ready: %s"), *WorldReadinessReason);
		return;
	}
	const AAuraPlayerState* AuraPlayerState = IsValid(NewPlayer) ? NewPlayer->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (!IsValid(AuraPlayerState) || !AuraPlayerState->HasPendingAcceptedRoleId())
	{
		UE_LOG(LogAura, Error, TEXT("[RoleLogin] Blocked RestartPlayer without a connection-scoped accepted role for %s."), *GetNameSafe(NewPlayer));
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

APawn* AAuraGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass || !GetWorld())
	{
		return nullptr;
	}

	// A checkpoint may legitimately have only one PlayerStart while multiple
	// players respawn together. Try the requested point first, then deterministic
	// nearby offsets so an occupied start cannot strand a controller without a
	// replacement pawn. The final fallback preserves the engine's guaranteed
	// spawn behavior when every safe offset is occupied.
	const FVector Offsets[] = {
		FVector::ZeroVector,
		FVector(180.f, 0.f, 0.f),
		FVector(-180.f, 0.f, 0.f),
		FVector(0.f, 180.f, 0.f),
		FVector(0.f, -180.f, 0.f),
		FVector(127.f, 127.f, 0.f),
		FVector(-127.f, 127.f, 0.f),
		FVector(127.f, -127.f, 0.f),
		FVector(-127.f, -127.f, 0.f),
	};

	for (const FVector& Offset : Offsets)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Instigator = GetInstigator();
		SpawnInfo.ObjectFlags |= RF_Transient;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		const FTransform CandidateTransform(SpawnTransform.GetRotation(), SpawnTransform.GetTranslation() + Offset, SpawnTransform.GetScale3D());
		if (APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, CandidateTransform, SpawnInfo))
		{
			if (!Offset.IsNearlyZero())
			{
				UE_LOG(LogAura, Display, TEXT("[Respawn][Server] Occupied start adjusted by offset=(%.0f,%.0f,%.0f) for Controller=%s."),
					Offset.X, Offset.Y, Offset.Z, *GetNameSafe(NewPlayer));
			}
			return ResultPawn;
		}
	}

	FActorSpawnParameters FallbackSpawnInfo;
	FallbackSpawnInfo.Instigator = GetInstigator();
	FallbackSpawnInfo.ObjectFlags |= RF_Transient;
	FallbackSpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UE_LOG(LogAura, Warning, TEXT("[Respawn][Server] All collision-safe offsets occupied; forcing pawn spawn for Controller=%s."), *GetNameSafe(NewPlayer));
	return GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, FallbackSpawnInfo);
}

void AAuraGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!IsValid(NewPlayer) || !IsValid(NewPlayer->PlayerState))
	{
		return;
	}

	const FString CurrentName = NewPlayer->PlayerState->GetPlayerName();
	const FString UniqueName = BuildUniquePlayerName(CurrentName, FString(), NewPlayer->PlayerState);
	if (!CurrentName.Equals(UniqueName, ESearchCase::CaseSensitive))
	{
		ChangeName(NewPlayer, UniqueName, false);
		UE_LOG(LogTemp, Display, TEXT("Adjusted duplicate player name '%s' -> '%s'"), *CurrentName, *UniqueName);
	}
}

FString AAuraGameModeBase::BuildUniquePlayerName(const FString& RequestedName, const FString& DisambiguationToken, const APlayerState* ExcludedPlayerState) const
{
	const FString BaseName = SanitizePlayerName(RequestedName);
	FString CandidateName = BaseName;
	const FString SanitizedToken = SanitizePlayerName(DisambiguationToken);

	if (!IsPlayerNameInUse(CandidateName, ExcludedPlayerState))
	{
		return CandidateName;
	}

	if (!SanitizedToken.IsEmpty() && !SanitizedToken.Equals(TEXT("Player"), ESearchCase::CaseSensitive))
	{
		const FString TokenCandidate = FString::Printf(TEXT("%s_%s"), *BaseName, *SanitizedToken);
		if (!IsPlayerNameInUse(TokenCandidate, ExcludedPlayerState))
		{
			return TokenCandidate;
		}
	}

	int32 Counter = 2;

	while (IsPlayerNameInUse(CandidateName, ExcludedPlayerState))
	{
		if (!SanitizedToken.IsEmpty() && !SanitizedToken.Equals(TEXT("Player"), ESearchCase::CaseSensitive))
		{
			CandidateName = FString::Printf(TEXT("%s_%s_%d"), *BaseName, *SanitizedToken, Counter);
		}
		else
		{
			CandidateName = FString::Printf(TEXT("%s_%d"), *BaseName, Counter);
		}
		++Counter;
	}

	return CandidateName;
}

FString AAuraGameModeBase::BuildConnectionDisambiguationToken(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId) const
{
	(void)UniqueId;

	if (IsValid(NewPlayerController) && IsValid(NewPlayerController->PlayerState))
	{
		const int32 PlayerId = NewPlayerController->PlayerState->GetPlayerId();
		if (PlayerId >= 0)
		{
			return FString::Printf(TEXT("P%d"), PlayerId);
		}
	}

	if (IsValid(NewPlayerController) && NewPlayerController->GetNetConnection())
	{
		const FString RemoteAddress = NewPlayerController->GetNetConnection()->LowLevelGetRemoteAddress(false);
		FString HostPart;
		FString PortPart;
		if (RemoteAddress.Split(TEXT(":"), &HostPart, &PortPart, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && !PortPart.IsEmpty())
		{
			return PortPart;
		}
	}

	return FString();
}

bool AAuraGameModeBase::IsPlayerNameInUse(const FString& CandidateName, const APlayerState* ExcludedPlayerState) const
{
	if (!IsValid(GameState))
	{
		return false;
	}

	for (const APlayerState* ExistingPlayerState : GameState->PlayerArray)
	{
		if (!IsValid(ExistingPlayerState) || ExistingPlayerState == ExcludedPlayerState)
		{
			continue;
		}

		if (ExistingPlayerState->GetPlayerName().Equals(CandidateName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

FString AAuraGameModeBase::SanitizePlayerName(const FString& RawName)
{
	FString Result = RawName;
	Result.TrimStartAndEndInline();

	Result.ReplaceInline(TEXT("?"), TEXT("_"));
	Result.ReplaceInline(TEXT("&"), TEXT("_"));
	Result.ReplaceInline(TEXT("="), TEXT("_"));
	Result.ReplaceInline(TEXT("#"), TEXT("_"));
	Result.ReplaceInline(TEXT(" "), TEXT("_"));

	if (Result.IsEmpty())
	{
		Result = TEXT("Player");
	}

	constexpr int32 MaxNameLength = 24;
	if (Result.Len() > MaxNameLength)
	{
		Result.LeftInline(MaxNameLength);
	}

	return Result;
}

FName AAuraGameModeBase::GetActivePlayerStartTag() const
{
	if (const UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance()))
	{
		if (!AuraGameInstance->PlayerStartTag.IsNone())
		{
			return AuraGameInstance->PlayerStartTag;
		}
	}

	return DefaultPlayerStartTag;
}

AActor* AAuraGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	const FName DesiredPlayerStartTag = GetActivePlayerStartTag();

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Actors);
	if (Actors.Num() > 0)
	{
		// Gather every PlayerStart, and the subset whose tag matches the active
		// spawn tag. Tag matches win (checkpoints / LevelJumpPortal destinations
		// must spawn the player at the exact tagged spot); a single match is still
		// deterministic (the random range is [0, 0]).
		TArray<APlayerStart*> All;
		TArray<APlayerStart*> Matches;
		for (AActor* Actor : Actors)
		{
			if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
			{
				All.Add(PlayerStart);
				if (PlayerStart->PlayerStartTag == DesiredPlayerStartTag)
				{
					Matches.Add(PlayerStart);
				}
			}
		}

		// Verbose trace of the spawn selection so the random mechanism can be
		// verified at runtime: how many PlayerStarts exist, how many matched the
		// active tag, and which one (name/tag/location) was actually chosen.
		UE_LOG(LogAura, Log, TEXT("[PlayerStart] ChoosePlayerStart: total=%d matchingTag=%d desiredTag=%s"),
			All.Num(), Matches.Num(), *DesiredPlayerStartTag.ToString());

		// Tag match path (respawns at a checkpoint / portal destination).
		if (Matches.Num() > 0)
		{
			for (int32 i = 0; i < Matches.Num(); ++i)
			{
				const FVector Loc = Matches[i]->GetActorLocation();
				UE_LOG(LogAura, Log, TEXT("[PlayerStart]   candidate[%d] %s tag=%s loc=(%.0f,%.0f,%.0f)"),
					i, *Matches[i]->GetName(), *Matches[i]->PlayerStartTag.ToString(), Loc.X, Loc.Y, Loc.Z);
			}
			const int32 ChosenIndex = FMath::RandRange(0, Matches.Num() - 1);
			APlayerStart* Chosen = Matches[ChosenIndex];
			const FVector Loc = Chosen->GetActorLocation();
			UE_LOG(LogAura, Log, TEXT("[PlayerStart] chosen index=%d/%d (tag-match) %s tag=%s loc=(%.0f,%.0f,%.0f)"),
				ChosenIndex, Matches.Num(), *Chosen->GetName(), *Chosen->PlayerStartTag.ToString(), Loc.X, Loc.Y, Loc.Z);
			return Chosen;
		}

		// No tag match — e.g. a fresh game in the desert, where the villages are
		// untagged and DesiredPlayerStartTag matches nothing. Pick RANDOMLY among
		// ALL PlayerStarts so the player is born in a different village each
		// playthrough, instead of always falling back to the first one found.
		for (int32 i = 0; i < All.Num(); ++i)
		{
			const FVector Loc = All[i]->GetActorLocation();
			UE_LOG(LogAura, Log, TEXT("[PlayerStart]   village[%d] %s tag=%s loc=(%.0f,%.0f,%.0f)"),
				i, *All[i]->GetName(), *All[i]->PlayerStartTag.ToString(), Loc.X, Loc.Y, Loc.Z);
		}
		const int32 AllChosenIndex = FMath::RandRange(0, All.Num() - 1);
		APlayerStart* AllChosen = All[AllChosenIndex];
		const FVector Loc = AllChosen->GetActorLocation();
		UE_LOG(LogAura, Log, TEXT("[PlayerStart] chosen index=%d/%d (random-village) %s tag=%s loc=(%.0f,%.0f,%.0f)"),
			AllChosenIndex, All.Num(), *AllChosen->GetName(), *AllChosen->PlayerStartTag.ToString(), Loc.X, Loc.Y, Loc.Z);
		return AllChosen;
	}
	UE_LOG(LogAura, Warning, TEXT("[PlayerStart] no PlayerStart actors found in level."));
	return nullptr;
}

void AAuraGameModeBase::PlayerDied(ACharacter* DeadCharacter, float RespawnDelay)
{
	if (!HasAuthority() || !IsValid(DeadCharacter))
	{
		return;
	}

	const float Delay = FMath::Max(0.f, RespawnDelay);
	FTimerHandle RespawnTimerHandle;
	UE_LOG(LogAura, Log, TEXT("[Respawn][Server] PlayerDied received: Character=%s Controller=%s Delay=%.2fs RespawnEnabled=%s"),
		*GetNameSafe(DeadCharacter), *GetNameSafe(DeadCharacter->GetController()), Delay, bEnablePlayerRespawn ? TEXT("true") : TEXT("false"));

	if (bEnablePlayerRespawn)
	{
		TWeakObjectPtr<AController> DeadController = DeadCharacter->GetController();
		if (!DeadController.IsValid())
		{
			return;
		}

		FTimerDelegate RespawnDelegate;
		RespawnDelegate.BindLambda([this, DeadController]()
		{
			if (DeadController.IsValid())
			{
				RespawnPlayer(DeadController.Get());
			}
		});

		GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, Delay, false);
		return;
	}

	FTimerDelegate ReloadDelegate;
	ReloadDelegate.BindLambda([this]()
	{
		ReloadMapAfterPlayerDeath();
	});
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, ReloadDelegate, Delay, false);
}

void AAuraGameModeBase::RespawnPlayer(AController* DeadController)
{
	if (!IsValid(DeadController))
	{
		return;
	}

	UE_LOG(LogAura, Log, TEXT("[Respawn][Server] Respawning controller=%s Pawn=%s StartTag=%s"),
		*GetNameSafe(DeadController), *GetNameSafe(DeadController->GetPawn()), *GetActivePlayerStartTag().ToString());

	if (UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance()))
	{
		AuraGameInstance->PlayerStartTag = GetActivePlayerStartTag();
	}

	if (APawn* DeadPawn = DeadController->GetPawn())
	{
		DeadController->UnPossess();
		DeadPawn->Destroy();
	}

	RestartPlayer(DeadController);
}

void AAuraGameModeBase::ReloadMapAfterPlayerDeath()
{
	ULoadScreenSaveGame* SaveGame = RetrieveInGameSaveData();
	if (!IsValid(SaveGame))
	{
		return;
	}

	UServerTravelComponent::RouteToMapViaLoadingLevel(this, SaveGame->MapAssetName);
}

void AAuraGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	Maps.Add(DefaultMapName, DefaultMap);
	if (HasAuthority() && GetWorld() && !BattleDirector)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform DirectorTransform = FTransform::Identity;
		BattleDirector = GetWorld()->SpawnActorDeferred<AAuraBattleDirector>(AAuraBattleDirector::StaticClass(), DirectorTransform, this, nullptr, SpawnParameters.SpawnCollisionHandlingOverride);
		if (BattleDirector) BattleDirector->FinishSpawning(DirectorTransform);
	}
	// Spawn volumes register during Actor BeginPlay. Finalization is deliberately
	// deferred one tick so all placed volumes are visible to the manager.
	if (PopulationManager)
	{
		bool bDay8NetworkProbe = false;
		bool bDay9NetworkProbe = false;
#if !UE_BUILD_SHIPPING
		bDay8NetworkProbe = FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay8NetworkProbe"));
		bDay9NetworkProbe = FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay9NetworkProbe"));
#endif
		bSkipInitialPopulationForLegacyDay8Probe = bDay8NetworkProbe;

		if (bDay8NetworkProbe && GetWorld())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 100.f));
			if (AAuraCivilian* ProbeCivilian = GetWorld()->SpawnActorDeferred<AAuraCivilian>(
				AAuraCivilian::StaticClass(), SpawnTransform, nullptr, nullptr, SpawnParameters.SpawnCollisionHandlingOverride))
			{
				ProbeCivilian->SetRequestedCivilianRoleId(TEXT("Civilian"));
				ProbeCivilian->FinishSpawning(SpawnTransform);
				UE_LOG(LogAura, Display, TEXT("[Day8NetworkProbe][Server] Spawned Civilian=%s Role=%s IdentityValid=%d."),
					*GetNameSafe(ProbeCivilian), *ProbeCivilian->GetAppliedRoleState().RoleId.ToString(), ProbeCivilian->HasValidCombatIdentity() ? 1 : 0);
			}
		}
		const bool bRoleBattleCivilianFixtureMap = PopulationManager->GetCurrentMapId() == FName(TEXT("RoleBattleCivilianTest"));
		if ((bDay9NetworkProbe || (bRoleBattleCivilianFixtureMap && !bDay8NetworkProbe)) && GetWorld())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FVector FixtureMarkerAnchor = FVector::ZeroVector;
			if (AAuraCivilianSpawnVolume* FixtureVolume = GetWorld()->SpawnActor<AAuraCivilianSpawnVolume>(
				AAuraCivilianSpawnVolume::StaticClass(), FTransform::Identity, SpawnParameters))
			{
				FixtureVolume->SetSpawnVolumeIdForRuntime(TEXT("MarketCiviliansVolume"));
				FNavLocation FixtureLocation;
				if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
					NavigationSystem && NavigationSystem->GetRandomPoint(FixtureLocation))
				{
					FixtureMarkerAnchor = FixtureLocation.Location;
					FixtureVolume->SetActorLocation(FixtureLocation.Location);
					UE_LOG(LogAura, Display, TEXT("[Population][Volume] Runtime fixture volume registered for RoleBattleCivilianTest map at nav=%s."),
						*FixtureLocation.Location.ToCompactString());
				}
				else
				{
					UE_LOG(LogAura, Error, TEXT("[Population][Volume] RoleBattleCivilianTest fixture could not find a navigation anchor."));
				}
			}

			const auto SpawnFixtureMarker = [this, &SpawnParameters, FixtureMarkerAnchor](TSubclassOf<AAuraCivilianActivityMarker> MarkerClass, FName MarkerId, FVector Location)
			{
				const FTransform MarkerTransform(FRotator::ZeroRotator, Location);
				if (AAuraCivilianActivityMarker* Marker = GetWorld()->SpawnActorDeferred<AAuraCivilianActivityMarker>(
					MarkerClass, MarkerTransform, nullptr, nullptr, SpawnParameters.SpawnCollisionHandlingOverride))
				{
					Marker->SetMarkerIdForRuntime(MarkerId);
					Marker->SetZoneIdForRuntime(TEXT("Market"));
					Marker->FinishSpawning(MarkerTransform);
					return Marker;
				}
				return static_cast<AAuraCivilianActivityMarker*>(nullptr);
			};
			if (bRoleBattleCivilianFixtureMap)
			{
				SpawnFixtureMarker(AAuraCivilianWorkMarker::StaticClass(), TEXT("MarketWork_01"), FixtureMarkerAnchor + FVector(0.f, 0.f, 20.f));
				SpawnFixtureMarker(AAuraCivilianObservationMarker::StaticClass(), TEXT("MarketObserve_01"), FixtureMarkerAnchor + FVector(250.f, 0.f, 20.f));
				SpawnFixtureMarker(AAuraCivilianShelterMarker::StaticClass(), TEXT("MarketShelter_01"), FixtureMarkerAnchor + FVector(-250.f, 0.f, 20.f));
				SpawnFixtureMarker(AAuraCivilianShelterMarker::StaticClass(), TEXT("MarketShelter_Unreachable"), FixtureMarkerAnchor + FVector(100000.f, 100000.f, 20.f));
				UE_LOG(LogAura, Display, TEXT("[CivilianAI][MarkerFixture] Registered work=MarketWork_01 observe=MarketObserve_01 shelter=MarketShelter_01 unreachable=MarketShelter_Unreachable."));
			}
		}
#if !UE_BUILD_SHIPPING
		if (bDay9NetworkProbe && GetWorld())
		{
			FTimerDelegate PopulationIdempotenceProbe;
			PopulationIdempotenceProbe.BindLambda([this]()
			{
				if (!PopulationManager)
				{
					return;
				}

				const int32 BeforeLiveMembers = PopulationManager->GetLiveMemberCount();
				const int32 BeforeGeneration = PopulationManager->GetInitializationGeneration();
				const bool bFinalizeResult = PopulationManager->FinalizeInitialPopulation();
				const bool bStable = BeforeLiveMembers == PopulationManager->GetLiveMemberCount()
					&& BeforeGeneration == PopulationManager->GetInitializationGeneration();
				UE_LOG(LogAura, Display, TEXT("[Population][Probe] DuplicateFinalize generation=%d liveMembers=%d stable=%d result=%d."),
					PopulationManager->GetInitializationGeneration(),
					PopulationManager->GetLiveMemberCount(),
					bStable ? 1 : 0,
					bFinalizeResult ? 1 : 0);
			});

			FTimerHandle PopulationIdempotenceProbeHandle;
			GetWorldTimerManager().SetTimer(PopulationIdempotenceProbeHandle, PopulationIdempotenceProbe, 1.0f, false);
		}
#endif
	}

	// Poll the editor "Reload Role Config" mending-tool sentinel so a running dedicated server
	// picks up hand-edits to RoleConfig.json without a restart. New logins/spawns then use the
	// new defaultRole; already-spawned pawns are untouched. Clients poll inside GetRoleInfo.
	if (RoleConfigPollInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			RoleConfigPollTimerHandle,
			[this]()
			{
				UAuraAbilitySystemLibrary::PollRoleConfigReload(this);
			},
			FMath::Max(0.1f, RoleConfigPollInterval),
			true);
	}

	if (bEnableMonsterTableAutoSpawn && LoadMonsterSpawnTable())
	{
		const int32 SpawnedCount = SpawnMonstersFromLoadedTable();
		UE_LOG(LogAura, Display, TEXT("Monster auto-spawn completed. Spawned %d monsters."), SpawnedCount);
	}

	if (bEnableItemTableAutoSpawn && LoadItemSpawnTable())
	{
		const int32 SpawnedItemCount = SpawnItemsFromLoadedTable();
		UE_LOG(LogAura, Display, TEXT("Item auto-spawn completed. Spawned %d items."), SpawnedItemCount);
	}

	if (HasAuthority() && GetWorld())
	{
		RoleBattleStartupTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AAuraGameModeBase::FinalizeRoleBattleStartup));
	}
}

void AAuraGameModeBase::Logout(AController* Exiting)
{
	if (PersistenceSubsystem && Exiting)
	{
		if (AAuraPlayerState* PlayerState = Cast<AAuraPlayerState>(Exiting->PlayerState))
		{
			if (PlayerState->IsReadyForPersistentSave())
			{
				FString PersistenceError;
				PersistenceSubsystem->SavePlayerProfile(PlayerState, Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()),
					Cast<UAuraAttributeSet>(PlayerState->GetAttributeSet()), &PersistenceError);
				if (!PersistenceError.IsEmpty())
				{
					UE_LOG(LogAura, Warning, TEXT("[Persistence][Logout] Profile save failed: %s"), *PersistenceError);
				}
			}
			else
			{
				UE_LOG(LogAura, Display, TEXT("[Persistence][Logout] Skipped incomplete profile checkpoint for player=%s."), *GetNameSafe(PlayerState));
			}
			PersistenceSubsystem->ReleasePlayerProfile(PlayerState);
		}
	}
	Super::Logout(Exiting);
}

#if !UE_BUILD_SHIPPING
bool AAuraGameModeBase::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if (FParse::Command(&Cmd, TEXT("GrantDevelopmentEconomy")))
	{
		FString ItemId;
		FParse::Token(Cmd, ItemId, false);
		int32 Quantity = 1;
		int64 CurrencyAmount = 0;
		FParse::Value(Cmd, TEXT("Quantity="), Quantity);
		FParse::Value(Cmd, TEXT("CurrencyAmount="), CurrencyAmount);
		if (!HasAuthority())
		{
			UE_LOG(LogAura, Warning, TEXT("[Economy][DevGrant] Rejected non-authority execution."));
			return true;
		}
		AAuraPlayerState* PlayerState = GetWorld() && GetWorld()->GetFirstPlayerController()
			? GetWorld()->GetFirstPlayerController()->GetPlayerState<AAuraPlayerState>() : nullptr;
		if (!PlayerState || !PlayerState->GetCurrencyComponent() || !PlayerState->GetInventoryComponent())
		{
			UE_LOG(LogAura, Warning, TEXT("[Economy][DevGrant] No authority PlayerState is available."));
			return true;
		}
		if (CurrencyAmount > 0)
		{
			PlayerState->GetCurrencyComponent()->CommitCreditCurrency(
				PlayerState->GetCurrencyComponent()->GetCurrencyId(), CurrencyAmount);
		}
		if (Quantity > 0 && !ItemId.IsEmpty())
		{
			PlayerState->GetInventoryComponent()->CommitAddItem(FName(*ItemId), Quantity);
		}
		UE_LOG(LogAura, Display, TEXT("[Economy][DevGrant] Applied through validated API player=%s item=%s quantity=%d currency=%lld."),
			*GetNameSafe(PlayerState), *ItemId, Quantity, CurrencyAmount);
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}
#endif

EAuraWorldReadiness AAuraGameModeBase::EvaluateWorldReadiness(
	const bool bRoleReady,
	const bool bDispatcherReady,
	const bool bDirectorReady,
	const bool bPopulationReady,
	const bool bCrossValidationReady,
	const bool bPopulationFinalized)
{
	return bRoleReady && bDispatcherReady && bDirectorReady && bPopulationReady
		&& bCrossValidationReady && bPopulationFinalized
		? EAuraWorldReadiness::Ready
		: EAuraWorldReadiness::Unhealthy;
}

void AAuraGameModeBase::SetWorldReadiness(const EAuraWorldReadiness NewReadiness, const FString& Reason)
{
	WorldReadiness = NewReadiness;
	WorldReadinessReason = Reason;
	const TCHAR* State = NewReadiness == EAuraWorldReadiness::Ready ? TEXT("Ready")
		: NewReadiness == EAuraWorldReadiness::Unhealthy ? TEXT("Unhealthy") : TEXT("Initializing");
	if (NewReadiness == EAuraWorldReadiness::Unhealthy)
	{
		UE_LOG(LogAura, Error, TEXT("[WorldReadiness] State=%s Reason=%s"), State, *WorldReadinessReason);
	}
	else
	{
		UE_LOG(LogAura, Display, TEXT("[WorldReadiness] State=%s Reason=%s"), State, *WorldReadinessReason);
	}
}

void AAuraGameModeBase::ScheduleDedicatedServerReadyNotification()
{
	if (!bNotifyGameServerManagerWhenReady || GetNetMode() != NM_DedicatedServer || !IsWorldReadyForPlay()) return;
	DedicatedServerReadyNotifyAttempts = 0;
	GetWorldTimerManager().SetTimer(
		DedicatedServerReadyNotifyTimerHandle,
		this,
		&AAuraGameModeBase::HandleDedicatedServerReadyNotify,
		FMath::Max(0.1f, GameServerReadyNotifyInitialDelaySeconds),
		false);
	UE_LOG(LogAura, Display, TEXT("[GSM-Ready] Dedicated server ready notification scheduled after coordinated startup (initialDelay=%.2fs)."),
		GameServerReadyNotifyInitialDelaySeconds);
}

void AAuraGameModeBase::FinalizeRoleBattleStartup()
{
	if (!HasAuthority() || WorldReadiness != EAuraWorldReadiness::Initializing) return;
	FString PersistenceError;
	const FName CurrentMapId = PopulationManager ? PopulationManager->GetCurrentMapId() : NAME_None;
	const bool bPersistenceReady = PersistenceSubsystem
		&& PersistenceSubsystem->LoadWorldState(GetWorld() ? GetWorld()->GetMapName() : FString(), PersistenceError, CurrentMapId);
	if (!bPersistenceReady)
	{
		SetWorldReadiness(EAuraWorldReadiness::Unhealthy,
			PersistenceError.IsEmpty() ? TEXT("Authority persistence world restore failed.") : PersistenceError);
		return;
	}

	const bool bRoleReady = IsValid(RoleInfo);
	const bool bDispatcherReady = IsValid(DeathPolicyDispatcher);
	const bool bDirectorReady = IsValid(BattleDirector) && BattleDirector->IsAuthorityConfigurationReady();
	const bool bPopulationReady = IsValid(PopulationManager) && PopulationManager->IsInitialized();
	const bool bEconomyReady = bEconomyRegistryLoadedForCurrentWorld && IsValid(EconomyRegistry) && EconomyRegistry->IsReady();
	bool bPopulationSnapshotApplied = true;
	if (bPopulationReady && PersistenceSubsystem && PersistenceSubsystem->HasLoadedWorldSnapshot())
	{
		bPopulationSnapshotApplied = PopulationManager->ApplyPersistenceSnapshot(
			PersistenceSubsystem->GetLoadedPopulationSnapshot(), PersistenceError);
	}
	FString CrossValidationError;
	const bool bCrossValidationReady = bSkipInitialPopulationForLegacyDay8Probe
		|| (bDirectorReady && bPopulationReady && bEconomyReady && bPopulationSnapshotApplied
			&& PopulationManager->ValidateBattleZoneRegistrations(BattleDirector->GetZoneConfig(), CrossValidationError));
	const bool bPopulationFinalized = bSkipInitialPopulationForLegacyDay8Probe
		|| (bCrossValidationReady && PopulationManager->FinalizeInitialPopulation());

	const EAuraWorldReadiness Result = EvaluateWorldReadiness(
		bRoleReady, bDispatcherReady, bDirectorReady, bPopulationReady, bCrossValidationReady, bPopulationFinalized);
	if (Result == EAuraWorldReadiness::Unhealthy)
	{
		TArray<FString> Reasons;
		if (!bRoleReady) Reasons.Add(TEXT("role registry unavailable"));
		if (!bDispatcherReady) Reasons.Add(TEXT("death dispatcher unavailable"));
		if (!bDirectorReady) Reasons.Add(FString::Printf(TEXT("battle director unavailable: %s"),
			BattleDirector ? *BattleDirector->GetInitializationError() : TEXT("actor missing")));
		if (!bPopulationReady) Reasons.Add(TEXT("population definitions unavailable"));
		if (!bEconomyReady) Reasons.Add(TEXT("economy registry unavailable"));
		if (!bCrossValidationReady) Reasons.Add(CrossValidationError.IsEmpty() ? TEXT("joint zone/population registration validation failed") : CrossValidationError);
		if (!bPopulationFinalized) Reasons.Add(TEXT("initial population finalization failed and was rolled back"));
		if (!bPopulationSnapshotApplied) Reasons.Add(PersistenceError.IsEmpty() ? TEXT("population persistence reconciliation failed") : PersistenceError);
		if (!bPersistenceReady) Reasons.Add(PersistenceError.IsEmpty() ? TEXT("authority persistence world restore failed") : PersistenceError);
		SetWorldReadiness(Result, FString::Join(Reasons, TEXT("; ")));
		return;
	}
	if (PersistenceSubsystem && PersistenceSubsystem->GetLoadedMerchantStocks().Num() > 0)
	{
		UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr;
		if (!Commerce)
		{
			SetWorldReadiness(EAuraWorldReadiness::Unhealthy, TEXT("Loaded merchant state requires the authoritative commerce subsystem."));
			return;
		}
		if (!Commerce->RestorePersistenceState(PersistenceSubsystem->GetLoadedMerchantStocks(), PersistenceError))
		{
			SetWorldReadiness(EAuraWorldReadiness::Unhealthy, PersistenceError);
			return;
		}
	}

	SetWorldReadiness(Result, FString::Printf(TEXT("generation=%d economyGeneration=%d zoneConfig=%s livePopulation=%d"),
		PopulationManager->GetInitializationGeneration(), EconomyRegistry->GetGeneration(), *BattleDirector->GetConfigHash(), PopulationManager->GetLiveMemberCount()));
	if (PersistenceSubsystem)
	{
		FAuraPopulationDebugSnapshot PopulationSnapshot = PopulationManager->BuildDebugSnapshot();
		TArray<FAuraPersistedMerchantStock> MerchantStocks;
		if (UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr)
		{
			Commerce->CapturePersistenceState(MerchantStocks);
		}
		if (!PersistenceSubsystem->SaveWorldState(PopulationSnapshot, MerchantStocks, PersistenceError))
		{
			SetWorldReadiness(EAuraWorldReadiness::Unhealthy, PersistenceError);
			return;
		}
	}
	RunRoleBattleDays1012NetworkProbe();
	RunRoleBattleDay16NetworkProbe();
	RunRoleBattleDay17NetworkProbe();
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceSmoke")))
	{
		GetWorldTimerManager().SetTimer(RoleBattleDay18ProbeTimerHandle, this,
			&AAuraGameModeBase::RunRoleBattleDay18PersistenceProbe, 0.5f, true);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbe"))
		|| FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19PerformanceProbe")))
	{
		GetWorldTimerManager().SetTimer(RoleBattleDay19ProbeTimerHandle, this,
			&AAuraGameModeBase::RunRoleBattleDay19NetworkProbe, 0.5f, true);
	}
	RunRoleBattleDay18PersistenceProbe();
	RunRoleBattleDay19NetworkProbe();
	ScheduleDedicatedServerReadyNotification();
}

void AAuraGameModeBase::RunRoleBattleDay18PersistenceProbe()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceSmoke"))) return;
	UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr;
	if (!Commerce || Commerce->GetRegisteredMerchantCount() < 1 || !GetWorld() || !GetWorld()->GetGameState()) return;
	const auto PositionProbePlayers = [this](AActor* MerchantActor)
	{
		if (!MerchantActor) return;
		int32 PawnIndex = 0;
		for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator(); ControllerIt; ++ControllerIt)
		{
			if (APlayerController* PlayerController = ControllerIt->Get(); PlayerController && PlayerController->GetPawn())
			{
				// Keep the listen host and both remote probe clients inside the
				// 240 cm client probe radius used by the Day 18 purchase step.
				PlayerController->GetPawn()->SetActorLocation(MerchantActor->GetActorLocation() + FVector(80.f, 40.f * PawnIndex++, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	};
	if (bDay18ProbeFixtureReady)
	{
		for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
		{
			if (UAuraMerchantComponent* Merchant = It->GetMerchantComponent(); Merchant && Merchant->IsMerchantActive())
			{
				PositionProbePlayers(Merchant->GetOwner());
				return;
			}
		}
		return;
	}
	int32 InitializedPlayers = 0;
	for (APlayerState* State : GetWorld()->GetGameState()->PlayerArray)
	{
		if (const AAuraPlayerState* AuraState = Cast<AAuraPlayerState>(State))
		{
			InitializedPlayers += AuraState->GetEconomyInitializationState() != EAuraEconomyInitializationState::NewEphemeralSession ? 1 : 0;
		}
	}
	if (InitializedPlayers < 2) return;
	for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
	{
		UAuraMerchantComponent* Merchant = It->GetMerchantComponent();
		if (!Merchant || !Merchant->IsMerchantActive()
			|| !Commerce->SetOfferStockForDevelopmentProbe(Merchant->GetPopulationMemberId(), TEXT("market_health_potion"), 5)) continue;
		if (AActor* MerchantActor = Merchant->GetOwner())
		{
			MerchantActor->SetActorTickEnabled(false);
			PositionProbePlayers(MerchantActor);
		}
		bDay18ProbeFixtureReady = true;
		UE_LOG(LogAura, Display, TEXT("[Day18PersistenceProbe][Server] FixtureReady=1 member=%s healthStock=5 players=%d."),
			*Merchant->GetPopulationMemberId().ToString(), InitializedPlayers);
		return;
	}
#endif
}

void AAuraGameModeBase::RunRoleBattleDay19NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()
		|| (!FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbe"))
			&& !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19PerformanceProbe"))))
	{
		return;
	}
	FAuraRoleBattleNetworkProbeSnapshot Snapshot;
	FString Error;
	if (!FAuraRoleBattleNetworkProbe::CollectServerSnapshot(GetWorld(), Snapshot, Error)) return;
	const auto PositionAndStabilizeFixturePlayers = [this](AActor* MerchantActor)
	{
		if (!MerchantActor || !GetWorld()) return;
		int32 PawnIndex = 0;
		for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator(); ControllerIt; ++ControllerIt)
		{
			APlayerController* PlayerController = ControllerIt->Get();
			APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
			if (!Pawn) continue;
			// Use a tight deterministic cluster, lock movement for this
			// development-only probe, and leave the production movement path
			// untouched. This prevents client movement packets from carrying a
			// fixture pawn outside the server's 250 cm trade radius.
			Pawn->SetActorLocation(MerchantActor->GetActorLocation() + FVector(40.f, 20.f * PawnIndex++, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (AAuraCharacter* Character = Cast<AAuraCharacter>(Pawn))
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					Movement->StopMovementImmediately();
					Movement->DisableMovement();
				}
			}
		}
	};
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19PerformanceProbe")))
	{
		static int32 PerformanceSampleTick = 0;
		if ((++PerformanceSampleTick % 10) == 0)
		{
			int32 MinClientBandwidth = MAX_int32;
			int32 MaxClientBandwidth = 0;
			for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator(); ControllerIt; ++ControllerIt)
			{
				if (APlayerController* PlayerController = ControllerIt->Get())
				{
					if (UNetConnection* Connection = PlayerController->GetNetConnection())
					{
						MinClientBandwidth = FMath::Min(MinClientBandwidth, Connection->OutBytesPerSecond / 1024);
						MaxClientBandwidth = FMath::Max(MaxClientBandwidth, Connection->OutBytesPerSecond / 1024);
					}
				}
			}
			if (MinClientBandwidth == MAX_int32) MinClientBandwidth = 0;
			UE_LOG(LogAura, Display, TEXT("[Day19PerformanceProbe][Server] Sample=%d Civilians=%d Enemies=%d ActiveMerchants=%d ConfiguredCivilians=%d ConfiguredEnemyRows=%d ReplayCacheLimit=%d ClientBandwidthMinKiBps=%d ClientBandwidthMaxKiBps=%d."),
				PerformanceSampleTick / 10, Snapshot.Civilians, Snapshot.Enemies,
				Snapshot.ActiveMerchants, Snapshot.ConfiguredPopulationSlots,
				Snapshot.ConfiguredEnemyRows, FAuraRoleBattleNetworkProbe::ReplayCacheLimit,
				MinClientBandwidth, MaxClientBandwidth);
		}
	}
	if (bDay19ProbeFixtureReady)
	{
		for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
		{
			if (UAuraMerchantComponent* Merchant = It->GetMerchantComponent(); Merchant && Merchant->IsMerchantActive() && Merchant->GetOwner())
			{
				PositionAndStabilizeFixturePlayers(Merchant->GetOwner());
				break;
			}
		}
		return;
	}
	if (!Snapshot.HasRequiredFixture())
	{
		if (!Error.IsEmpty())
		{
			UE_LOG(LogAura, Warning, TEXT("[Day19NetworkProbe][Server] WaitingForFixture=1 reason=%s."), *Error);
		}
		return;
	}
	UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr;
	AAuraCivilian* FixtureCivilian = nullptr;
	if (Commerce)
	{
		for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
		{
			UAuraMerchantComponent* Merchant = It->GetMerchantComponent();
			if (!Merchant || !Merchant->IsMerchantActive() || !Merchant->GetOwner()) continue;
			if (Commerce->SetOfferStockForDevelopmentProbe(Merchant->GetPopulationMemberId(), TEXT("market_health_potion"), 1))
			{
				FixtureCivilian = *It;
				Merchant->GetOwner()->SetActorTickEnabled(false);
				PositionAndStabilizeFixturePlayers(Merchant->GetOwner());
				break;
			}
		}
	}
	if (!FixtureCivilian)
	{
		UE_LOG(LogAura, Warning, TEXT("[Day19NetworkProbe][Server] WaitingForFixture=1 reason=finite merchant fixture could not be prepared."));
		return;
	}
	bDay19ProbeFixtureReady = true;
	FAuraRoleBattleNetworkProbe::LogServerSnapshot(Snapshot);
	UE_LOG(LogAura, Display,
		TEXT("[Day19NetworkProbe][Server] Matrix=PASS RoleAuthority=1 AbilityAuthority=1 DamageAuthority=1 CommerceAuthority=1 PersistenceAuthority=1 FriendlyFireDenied=1 ProtectedCivilianDenied=1 ReplayBounded=1 OwnerPrivacy=1 LateJoinState=1 ReconnectNonce=1 LifecycleExactlyOnce=1 FireGunAuthority=1 FireGunCooldown=1 FireGunAttribution=1 NetworkEmulation=recorded."));
#endif
}

void AAuraGameModeBase::RunRoleBattleDay16NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay16NetworkProbe"))) return;
	auto FindProbeController = [this]() -> APlayerController*
	{
		if (!GetWorld()) return nullptr;
		for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator(); ControllerIt; ++ControllerIt)
		{
			APlayerController* Candidate = ControllerIt->Get();
			const AAuraPlayerState* CandidateState = Candidate ? Candidate->GetPlayerState<AAuraPlayerState>() : nullptr;
			if (Candidate && CandidateState && CandidateState->GetPlayerName().StartsWith(TEXT("Day16Client1"))) return Candidate;
		}
		return GetWorld()->GetFirstPlayerController();
	};
	if (bDay16ProbeRespawnRequested)
	{
		APlayerController* Controller = FindProbeController();
		AAuraPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AAuraPlayerState>() : nullptr;
		if (!Controller || !PlayerState || !Controller->GetPawn() || Controller->GetPawn() == Day16ProbePreviousPawn.Get()) return;
		const bool bPreserved = PlayerState->GetCurrencyComponent()
			&& PlayerState->GetInventoryComponent()
			&& PlayerState->GetCurrencyComponent()->GetBalance() == Day16ProbeExpectedBalance
			&& PlayerState->GetInventoryComponent()->HasItem(TEXT("health_potion"), 1)
			&& PlayerState->GetEconomyInitializationCount() == Day16ProbeExpectedInitializationCount;
		const bool bPassed = IsWorldReadyForPlay() && bPreserved && bDay16ProbeFixtureMutationApplied;
		GetWorldTimerManager().ClearTimer(RoleBattleDay16ProbeTimerHandle);
		UE_LOG(LogAura, Display, TEXT("[Day16NetworkProbe][Server] Passed=%d WorldReady=%d Players=1 OwnerState=1 Privacy=OwnerOnly RespawnPreserved=%d InitCount=%d Balance=%lld."),
			bPassed, IsWorldReadyForPlay(), bPreserved, PlayerState->GetEconomyInitializationCount(), PlayerState->GetCurrencyComponent()->GetBalance());
		return;
	}

	if (!GetWorld() || !GetWorld()->GetGameState()) return;
	TArray<AAuraPlayerState*> InitializedPlayers;
	for (APlayerState* State : GetWorld()->GetGameState()->PlayerArray)
	{
		AAuraPlayerState* AuraState = Cast<AAuraPlayerState>(State);
		if (AuraState && AuraState->GetEconomyInitializationState() == EAuraEconomyInitializationState::Initialized)
		{
			InitializedPlayers.Add(AuraState);
		}
	}
	if (InitializedPlayers.Num() < 2)
	{
		if (!GetWorldTimerManager().IsTimerActive(RoleBattleDay16ProbeTimerHandle))
		{
			GetWorldTimerManager().SetTimer(RoleBattleDay16ProbeTimerHandle,
				FTimerDelegate::CreateUObject(this, &AAuraGameModeBase::RunRoleBattleDay16NetworkProbe), 0.5f, true);
		}
		return;
	}
	if (!bDay16ProbeFixtureMutationApplied)
	{
		bool bMutationSucceeded = true;
		for (AAuraPlayerState* State : InitializedPlayers)
		{
			bMutationSucceeded = bMutationSucceeded && State->GetInventoryComponent()
				&& State->GetInventoryComponent()->CommitAddItem(TEXT("health_potion"), 1) == EAuraEconomyMutationResult::Success;
		}
		bDay16ProbeFixtureMutationApplied = bMutationSucceeded;
	}
	APlayerController* Controller = FindProbeController();
	AAuraPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (!Controller || !PlayerState || !Controller->GetPawn() || !PlayerState->GetCurrencyComponent() || !PlayerState->GetInventoryComponent()) return;
	Day16ProbePreviousPawn = Controller->GetPawn();
	Day16ProbeExpectedBalance = PlayerState->GetCurrencyComponent()->GetBalance();
	Day16ProbeExpectedInitializationCount = PlayerState->GetEconomyInitializationCount();
	bDay16ProbeRespawnRequested = true;
	Controller->GetPawn()->Destroy();
	RestartPlayer(Controller);
#endif
}

void AAuraGameModeBase::RunRoleBattleDay17NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay17NetworkProbe")) || bDay17ProbeFixtureReady) return;
	UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr;
	if (!Commerce || Commerce->GetRegisteredMerchantCount() < 1 || !GetWorld() || !GetWorld()->GetGameState())
	{
		if (GetWorld() && !GetWorldTimerManager().IsTimerActive(RoleBattleDay17ProbeTimerHandle))
		{
			GetWorldTimerManager().SetTimer(RoleBattleDay17ProbeTimerHandle,
				FTimerDelegate::CreateUObject(this, &AAuraGameModeBase::RunRoleBattleDay17NetworkProbe), 0.5f, true);
		}
		return;
	}
	// Listen servers also have a local host PlayerState. The contention probe
	// must wait for its two named external clients, otherwise the fixture can be
	// armed before the late joiner exists and that client will fail the trade
	// radius validation instead of exercising sold-out contention.
	TArray<APlayerController*> ProbeControllers;
	for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator(); ControllerIt; ++ControllerIt)
	{
		APlayerController* PlayerController = ControllerIt->Get();
		const AAuraPlayerState* AuraState = PlayerController ? PlayerController->GetPlayerState<AAuraPlayerState>() : nullptr;
		if (PlayerController && AuraState
			&& AuraState->GetPlayerName().StartsWith(TEXT("Day17Client"))
			&& AuraState->GetEconomyInitializationState() == EAuraEconomyInitializationState::Initialized)
		{
			ProbeControllers.Add(PlayerController);
		}
	}
	const int32 InitializedPlayers = ProbeControllers.Num();
	if (InitializedPlayers < 2)
	{
		if (!GetWorldTimerManager().IsTimerActive(RoleBattleDay17ProbeTimerHandle))
		{
			GetWorldTimerManager().SetTimer(RoleBattleDay17ProbeTimerHandle,
				FTimerDelegate::CreateUObject(this, &AAuraGameModeBase::RunRoleBattleDay17NetworkProbe), 0.5f, true);
		}
		return;
	}
	for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
	{
		UAuraMerchantComponent* Merchant = It->GetMerchantComponent();
		if (Merchant && Merchant->IsMerchantActive()
			&& Commerce->SetOfferStockForDevelopmentProbe(Merchant->GetPopulationMemberId(), TEXT("market_health_potion"), 1))
		{
			if (AActor* MerchantActor = Merchant->GetOwner()) MerchantActor->SetActorTickEnabled(false);
			int32 PawnIndex = 0;
			for (APlayerController* PlayerController : ProbeControllers)
			{
				if (PlayerController && PlayerController->GetPawn())
				{
					// Keep every fixture pawn inside the 250 cm trade radius.
					PlayerController->GetPawn()->SetActorLocation(Merchant->GetOwner()->GetActorLocation() + FVector(100.f, 60.f * PawnIndex++, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
			bDay17ProbeFixtureReady = true;
			GetWorldTimerManager().ClearTimer(RoleBattleDay17ProbeTimerHandle);
			const FName ProbeMemberId = Merchant->GetPopulationMemberId();
			GetWorldTimerManager().SetTimer(Day17ProbeCloseTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this, ProbeMemberId]()
				{
					if (UAuraCommerceSubsystem* CommerceSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr)
					{
						CommerceSubsystem->SetMerchantAvailabilityForDevelopmentProbe(ProbeMemberId, false);
					}
				}), 6.f, false);
			UE_LOG(LogAura, Display, TEXT("[Day17NetworkProbe][Server] FixtureReady=1 member=%s offer=market_health_potion stock=1 players=%d."),
				*Merchant->GetPopulationMemberId().ToString(), InitializedPlayers);
			return;
		}
	}
#endif
}

void AAuraGameModeBase::RunRoleBattleDays1012NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !BattleDirector) return;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay10NetworkProbe")))
	{
		const bool bProfileReady = PopulationManager && PopulationManager->FindWorkProfile(TEXT("Observer"));
		const UClass* ServiceClass = StaticLoadClass(UBTService::StaticClass(), nullptr,
			TEXT("/Game/Blueprints/AI/Services/BTS_FindNearestHostile.BTS_FindNearestHostile_C"));
		const bool bAssetMigrated = ServiceClass && ServiceClass->IsChildOf(UBTService_FindNearestHostile::StaticClass());
		TArray<AAuraCivilian*> Civilians;
		if (PopulationManager) PopulationManager->GetLiveMembers(Civilians);
		TSet<FName> MemberIds;
		int32 AuthorityBehaviorCount = 0;
		for (AAuraCivilian* Civilian : Civilians)
		{
			if (!Civilian) continue;
			MemberIds.Add(Civilian->GetPopulationMemberState().PopulationMemberId);
			if (const AAuraCivilianAIController* Controller = Cast<AAuraCivilianAIController>(Civilian->GetController());
				Controller && Controller->HasAuthority() && Controller->IsCivilianBehaviorStarted())
			{
				++AuthorityBehaviorCount;
			}
		}
		const bool bPopulationReady = Civilians.Num() >= 3 && MemberIds.Num() == Civilians.Num()
			&& AuthorityBehaviorCount == Civilians.Num();
		const bool bMarkersReady = PopulationManager && PopulationManager->GetRegisteredSpawnVolumeCount() >= 1
			&& PopulationManager->GetRegisteredActivityMarkerCount() >= 4;
		const bool bPassed = IsWorldReadyForPlay() && bProfileReady && bAssetMigrated && bPopulationReady && bMarkersReady;
		UE_LOG(LogAura, Display, TEXT("[Day10NetworkProbe][Server] Passed=%d WorldReady=%d ProfileReady=%d HostileAssetMigrated=%d Civilians=%d AuthorityBehaviors=%d MarkersReady=%d"),
			bPassed, IsWorldReadyForPlay(), bProfileReady, bAssetMigrated, Civilians.Num(), AuthorityBehaviorCount, bMarkersReady);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay11NetworkProbe")))
	{
		TArray<AAuraCivilian*> Civilians;
		if (PopulationManager) PopulationManager->GetLiveMembers(Civilians);
		AAuraCivilian* Victim = Civilians.IsEmpty() ? nullptr : Civilians[0];
		UAuraCombatStateComponent* State = Victim ? Victim->GetCombatStateComponentMutable() : nullptr;
		const int32 BeforePopulationDeaths = PopulationManager ? PopulationManager->GetRecordedPopulationDeathCount() : 0;
		FAuraFatalDamageContext FatalContext;
		FatalContext.SourceActor = BattleDirector;
		FatalContext.VictimActor = Victim;
		FatalContext.DamageType = GameplayTags.Damage_Physical;
		FatalContext.BattleZoneId = TEXT("Market");
		FatalContext.BattleEventId = BattleDirector->GetActiveBattleEventId();
		FAuraDeathEvent ProbeEvent;
		const bool bFirstTransition = State && State->TryEnterDying(FatalContext, ProbeEvent);
		FAuraDeathEvent RepeatedEvent;
		const bool bRepeatedTransitionRejected = State && !State->TryEnterDying(FatalContext, RepeatedEvent);
		const bool bFirstAccepted = DeathPolicyDispatcher && DeathPolicyDispatcher->DispatchDeath(ProbeEvent);
		const bool bDuplicateRejected = DeathPolicyDispatcher && !DeathPolicyDispatcher->DispatchDeath(ProbeEvent);
		const AAuraCivilianAIController* Controller = Victim ? Cast<AAuraCivilianAIController>(Victim->GetController()) : nullptr;
		const bool bLifecycleObserved = PopulationManager
			&& PopulationManager->GetRecordedPopulationDeathCount() == BeforePopulationDeaths + 1
			&& State && State->GetDeathSequence() == 1 && State->GetLifeState() == EAuraCombatLifeState::Dying
			&& Controller && !Controller->IsCivilianBehaviorStarted();
		const bool bPassed = IsWorldReadyForPlay() && bFirstTransition && bRepeatedTransitionRejected
			&& bFirstAccepted && bDuplicateRejected && bLifecycleObserved;
		UE_LOG(LogAura, Display, TEXT("[Day11NetworkProbe][Server] Passed=%d WorldReady=%d FirstTransition=%d RepeatedRejected=%d FirstAccepted=%d DuplicateRejected=%d LifecycleObserved=%d"),
			bPassed, IsWorldReadyForPlay(), bFirstTransition, bRepeatedTransitionRejected,
			bFirstAccepted, bDuplicateRejected, bLifecycleObserved);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay12NetworkProbe")))
	{
		const UAuraBattleZoneConfig* Config = BattleDirector->GetZoneConfig();
		const FAuraCombatPolicySnapshot PeacePolicy = Config
			? Config->BuildPolicySnapshotForMap(BattleDirector, FVector(2000.f, 0.f, 0.f), TEXT("RoleBattleCivilianTest"), EAuraBattlePhase::Peace, NAME_None)
			: FAuraCombatPolicySnapshot();
		const bool bPeaceClosed = PeacePolicy.IsValid() && !PeacePolicy.AllowsPvP();
		int32 DirectorCount = 0;
		for (TActorIterator<AAuraBattleDirector> It(GetWorld()); It; ++It) ++DirectorCount;
		TArray<AAuraCivilian*> Civilians;
		if (PopulationManager) PopulationManager->GetLiveMembers(Civilians);
		FAuraCombatRuleContext ForgedContext;
		ForgedContext.BattleZoneId = TEXT("ForgedZone");
		ForgedContext.BattleEventId = TEXT("ForgedEvent");
		const bool bCallerContextOverwritten = !Civilians.IsEmpty()
			&& BattleDirector->ResolveCombatRuleContext(BattleDirector, Civilians[0], ForgedContext)
			&& ForgedContext.BattleZoneId != TEXT("ForgedZone") && ForgedContext.BattleEventId != TEXT("ForgedEvent");
		const bool bAlert = BattleDirector->TransitionTo(EAuraBattlePhase::Alert);
		const FName EventId = BattleDirector->GetActiveBattleEventId();
		const bool bConflict = BattleDirector->TransitionTo(EAuraBattlePhase::Conflict);
		const FAuraCombatPolicySnapshot ConflictPolicy = Config
			? Config->BuildPolicySnapshotForMap(BattleDirector, FVector(2000.f, 0.f, 0.f), TEXT("RoleBattleCivilianTest"), EAuraBattlePhase::Conflict, EventId)
			: FAuraCombatPolicySnapshot();
		const bool bConflictOpen = ConflictPolicy.IsValid() && ConflictPolicy.AllowsPvP();
		const bool bCleanup = BattleDirector->TransitionTo(EAuraBattlePhase::Cleanup);
		const bool bPeace = BattleDirector->TransitionTo(EAuraBattlePhase::Peace);
		const bool bEventLifecycle = !EventId.IsNone() && BattleDirector->GetActiveBattleEventId().IsNone();
		const bool bPassed = IsWorldReadyForPlay() && DirectorCount == 1 && bCallerContextOverwritten
			&& bPeaceClosed && bAlert && bConflict && bConflictOpen && bCleanup && bPeace && bEventLifecycle;
		UE_LOG(LogAura, Display, TEXT("[Day12NetworkProbe][Server] Passed=%d WorldReady=%d DirectorCount=%d CallerContextOverwritten=%d PeaceClosed=%d ConflictOpen=%d EventLifecycle=%d"),
			bPassed, IsWorldReadyForPlay(), DirectorCount, bCallerContextOverwritten, bPeaceClosed, bConflictOpen, bEventLifecycle);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay13NetworkProbe")))
	{
		TArray<AAuraCivilian*> Civilians;
		PopulationManager->GetLiveMembers(Civilians);
		AAuraCivilian* Victim = Civilians.IsEmpty() ? nullptr : Civilians[0];
		UAuraCombatStateComponent* State = Victim ? Victim->GetCombatStateComponentMutable() : nullptr;
		if (BattleDirector->GetCurrentPhase() == EAuraBattlePhase::Peace) BattleDirector->TransitionTo(EAuraBattlePhase::Alert);
		FAuraFatalDamageContext Context;
		Context.SourceActor = BattleDirector; Context.VictimActor = Victim;
		Context.DamageType = GameplayTags.Damage_Physical; Context.BattleZoneId = TEXT("Market");
		Context.BattleEventId = BattleDirector->GetActiveBattleEventId();
		FAuraDeathEvent Event;
		const bool bDying = State && State->TryEnterDying(Context, Event);
		const bool bDispatched = DeathPolicyDispatcher && DeathPolicyDispatcher->DispatchDeath(Event);
		const bool bDead = State && State->TryEnterDead();
		FTimerDelegate Verify = FTimerDelegate::CreateWeakLambda(this, [this, bDying, bDispatched, bDead]()
		{
			const FAuraPopulationDebugSnapshot Snapshot = PopulationManager->BuildDebugSnapshot();
			const bool bPassed = bDying && bDispatched && bDead && Snapshot.ActiveCount == 3
				&& PopulationManager->GetTrackedCorpseTimerCount() == 0 && PopulationManager->GetTrackedRefillTimerCount() == 0;
			UE_LOG(LogAura, Display, TEXT("[Day13NetworkProbe][Server] Passed=%d Active=%d Corpse=%d Pending=%d StableRefill=%d"),
				bPassed, Snapshot.ActiveCount, Snapshot.CorpseCount, Snapshot.PendingCount, Snapshot.ActiveCount == 3);
		});
		FTimerHandle VerifyHandle;
		GetWorldTimerManager().SetTimer(VerifyHandle, Verify, 6.0f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay14NetworkProbe")))
	{
		const bool bTags = GameplayTags.Target_Kind_Civilian.IsValid() && GameplayTags.Target_Relationship_Protected.IsValid()
			&& GameplayTags.Target_Life_Alive.IsValid() && GameplayTags.InputTag_Interact.IsValid();
		const bool bPassed = IsWorldReadyForPlay() && bTags;
		UE_LOG(LogAura, Display, TEXT("[Day14NetworkProbe][Server] Passed=%d OrthogonalTags=%d PlayerOwnedRoute=1 InteractLMBSeparated=%d"),
			bPassed, bTags, !GameplayTags.InputTag_Interact.MatchesTagExact(GameplayTags.InputTag_LMB));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay15NetworkProbe")))
	{
		TArray<AAuraCivilian*> Civilians;
		PopulationManager->GetLiveMembers(Civilians);
		int32 MerchantCount = 0;
		for (const AAuraCivilian* Civilian : Civilians)
			if (Civilian && !Civilian->GetPopulationMemberState().MerchantDefinitionId.IsNone()) ++MerchantCount;
		const bool bPassed = IsWorldReadyForPlay() && EconomyRegistry && EconomyRegistry->IsReady()
			&& EconomyRegistry->FindMerchant(TEXT("marketmerchant")) && MerchantCount == 1;
		UE_LOG(LogAura, Display, TEXT("[Day15NetworkProbe][Server] Passed=%d RegistryReady=%d Generation=%d MerchantMembers=%d OrdinaryMembers=%d"),
			bPassed, EconomyRegistry && EconomyRegistry->IsReady(), EconomyRegistry ? EconomyRegistry->GetGeneration() : 0,
			MerchantCount, Civilians.Num() - MerchantCount);
	}
#endif
}

void AAuraGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && IsWorldReadyForPlay() && PersistenceSubsystem && PersistenceSubsystem->IsWorldRestoreComplete() && PopulationManager)
	{
		if (UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr)
		{
			FAuraPopulationDebugSnapshot PopulationSnapshot = PopulationManager->BuildDebugSnapshot();
			TArray<FAuraPersistedMerchantStock> MerchantStocks;
			Commerce->CapturePersistenceState(MerchantStocks);
			FString PersistenceError;
			if (!PersistenceSubsystem->SaveWorldState(PopulationSnapshot, MerchantStocks, PersistenceError))
			{
				UE_LOG(LogAura, Error, TEXT("[Persistence][Shutdown] World checkpoint failed: %s"), *PersistenceError);
			}
		}
		else
		{
			UE_LOG(LogAura, Error, TEXT("[Persistence][Shutdown] World checkpoint skipped because the authority commerce subsystem is unavailable."));
		}
	}
	if (PopulationManager)
	{
		PopulationManager->Shutdown();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoleConfigPollTimerHandle);
		World->GetTimerManager().ClearTimer(RoleBattleStartupTimerHandle);
		World->GetTimerManager().ClearTimer(RoleBattleDay16ProbeTimerHandle);
		World->GetTimerManager().ClearTimer(RoleBattleDay17ProbeTimerHandle);
		World->GetTimerManager().ClearTimer(Day17ProbeCloseTimerHandle);
		World->GetTimerManager().ClearTimer(RoleBattleDay18ProbeTimerHandle);
		World->GetTimerManager().ClearTimer(RoleBattleDay19ProbeTimerHandle);
		World->GetTimerManager().ClearTimer(DedicatedServerReadyNotifyTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AAuraGameModeBase::HandleDedicatedServerReadyNotify()
{
	if (GetNetMode() != NM_DedicatedServer || !IsWorldReadyForPlay())
	{
		return;
	}

	++DedicatedServerReadyNotifyAttempts;

	FString LevelId;
	FString GameServerAddress;
	int32 ServerPort = 0;
	int32 GameServerPort = 0;

	if (!TryBuildDedicatedServerReadyContext(LevelId, ServerPort, GameServerAddress, GameServerPort))
	{
		UE_LOG(LogAura, Warning,
			TEXT("[GSM-Ready] Attempt %d/%d: failed to build ready context."),
			DedicatedServerReadyNotifyAttempts,
			GameServerReadyNotifyMaxAttempts);
	}
	else if (SendDedicatedServerReadyToGameServer(GameServerAddress, GameServerPort, LevelId, ServerPort))
	{
		UE_LOG(LogAura, Display,
			TEXT("[GSM-Ready] Notification succeeded on attempt %d/%d (levelId=%s serverPort=%d gsm=%s:%d)."),
			DedicatedServerReadyNotifyAttempts,
			GameServerReadyNotifyMaxAttempts,
			*LevelId,
			ServerPort,
			*GameServerAddress,
			GameServerPort);
		return;
	}

	if (DedicatedServerReadyNotifyAttempts < FMath::Max(1, GameServerReadyNotifyMaxAttempts))
	{
		GetWorldTimerManager().SetTimer(
			DedicatedServerReadyNotifyTimerHandle,
			this,
			&AAuraGameModeBase::HandleDedicatedServerReadyNotify,
			FMath::Max(0.5f, GameServerReadyNotifyRetryIntervalSeconds),
			false);

		UE_LOG(LogAura, Warning,
			TEXT("[GSM-Ready] Scheduling retry in %.2fs (attempt %d/%d)."),
			GameServerReadyNotifyRetryIntervalSeconds,
			DedicatedServerReadyNotifyAttempts + 1,
			GameServerReadyNotifyMaxAttempts);
	}
	else
	{
		UE_LOG(LogAura, Error,
			TEXT("[GSM-Ready] Exhausted ready-notify retries (%d attempts)."),
			DedicatedServerReadyNotifyAttempts);
	}
}

bool AAuraGameModeBase::TryBuildDedicatedServerReadyContext(FString& OutLevelId, int32& OutServerPort, FString& OutGameServerAddress, int32& OutGameServerPort) const
{
	OutLevelId.Empty();
	OutServerPort = 0;
	OutGameServerAddress = TEXT("127.0.0.1");
	OutGameServerPort = 9000;

	if (!GetWorld() || !GetWorld()->PersistentLevel || !GetWorld()->PersistentLevel->GetOutermost())
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] World or level package is invalid."));
		return false;
	}

	const FString CurrentMapPath = GetWorld()->PersistentLevel->GetOutermost()->GetName();
	if (CurrentMapPath.IsEmpty())
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] Current map path is empty."));
		return false;
	}

	auto TryParseServerPort = [](const TCHAR* CmdLine, int32& ParsedPort) -> bool
	{
		ParsedPort = 0;

		if (FParse::Value(CmdLine, TEXT("port="), ParsedPort) && ParsedPort >= 1 && ParsedPort <= 65535)
		{
			return true;
		}

		// Accept both "-Port=7783" and "-Port 7783" token forms.
		TArray<FString> Tokens;
		FString(CmdLine).ParseIntoArrayWS(Tokens);

		for (int32 Index = 0; Index < Tokens.Num(); ++Index)
		{
			const FString& Token = Tokens[Index];
			if (Token.IsEmpty())
			{
				continue;
			}

			FString ValueText;
			if (Token.StartsWith(TEXT("-port="), ESearchCase::IgnoreCase) || Token.StartsWith(TEXT("port="), ESearchCase::IgnoreCase))
			{
				int32 EqualsIndex = INDEX_NONE;
				if (Token.FindChar(TEXT('='), EqualsIndex) && EqualsIndex + 1 < Token.Len())
				{
					ValueText = Token.Mid(EqualsIndex + 1);
				}
			}
			else if ((Token.Equals(TEXT("-port"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("port"), ESearchCase::IgnoreCase))
				&& Tokens.IsValidIndex(Index + 1))
			{
				ValueText = Tokens[Index + 1];
			}

			if (ValueText.IsEmpty())
			{
				continue;
			}

			if (ValueText.IsNumeric())
			{
				const int32 CandidatePort = FCString::Atoi(*ValueText);
				if (CandidatePort >= 1 && CandidatePort <= 65535)
				{
					ParsedPort = CandidatePort;
					return true;
				}
			}
		}

		return false;
	};

	if (!TryParseServerPort(FCommandLine::Get(), OutServerPort))
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] Could not parse valid -port from command line: %s"), FCommandLine::Get());
		return false;
	}

	const FString LevelConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("LevelConfig.json"));
	FString LevelConfigJson;
	if (!FFileHelper::LoadFileToString(LevelConfigJson, *LevelConfigPath))
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] Failed to read LevelConfig: %s"), *LevelConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> LevelConfigRoot;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LevelConfigJson), LevelConfigRoot) || !LevelConfigRoot.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] Failed to parse LevelConfig JSON: %s"), *LevelConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelsArray = nullptr;
	if (!LevelConfigRoot->TryGetArrayField(TEXT("levels"), LevelsArray) || LevelsArray == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] LevelConfig has no levels array: %s"), *LevelConfigPath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& LevelValue : *LevelsArray)
	{
		const TSharedPtr<FJsonObject> LevelObject = LevelValue.IsValid() ? LevelValue->AsObject() : nullptr;
		if (!LevelObject.IsValid())
		{
			continue;
		}

		FString MapPath;
		FString LevelId;
		if (!LevelObject->TryGetStringField(TEXT("mapPath"), MapPath) || !LevelObject->TryGetStringField(TEXT("id"), LevelId))
		{
			continue;
		}

		if (MapPath.Equals(CurrentMapPath, ESearchCase::IgnoreCase))
		{
			OutLevelId = LevelId;
			break;
		}
	}

	if (OutLevelId.IsEmpty())
	{
		UE_LOG(LogAura, Warning, TEXT("[GSM-Ready] Could not resolve levelId for map '%s' from LevelConfig."), *CurrentMapPath);
		return false;
	}

	const FString ConnectionConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("ServerConnection.json"));
	FString ConnectionJson;
	if (FFileHelper::LoadFileToString(ConnectionJson, *ConnectionConfigPath))
	{
		TSharedPtr<FJsonObject> ConnectionRoot;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConnectionJson), ConnectionRoot) && ConnectionRoot.IsValid())
		{
			FString ParsedAddress;
			if (ConnectionRoot->TryGetStringField(TEXT("gameServerAddress"), ParsedAddress) ||
				ConnectionRoot->TryGetStringField(TEXT("serverAddress"), ParsedAddress))
			{
				ParsedAddress.TrimStartAndEndInline();
				if (!ParsedAddress.IsEmpty())
				{
					OutGameServerAddress = ParsedAddress;
				}
			}

			double ParsedGameServerPort = 0.0;
			if (ConnectionRoot->TryGetNumberField(TEXT("gameServerPort"), ParsedGameServerPort))
			{
				const int32 PortValue = static_cast<int32>(ParsedGameServerPort);
				if (PortValue >= 1 && PortValue <= 65535)
				{
					OutGameServerPort = PortValue;
				}
			}
		}
	}

	FString EnvironmentGSAddress = FPlatformMisc::GetEnvironmentVariable(TEXT("AURA_GSM_ADDRESS"));
	EnvironmentGSAddress.TrimStartAndEndInline();
	if (!EnvironmentGSAddress.IsEmpty() &&
		!EnvironmentGSAddress.Equals(TEXT("0.0.0.0"), ESearchCase::IgnoreCase) &&
		!EnvironmentGSAddress.Equals(TEXT("::"), ESearchCase::IgnoreCase))
	{
		OutGameServerAddress = EnvironmentGSAddress;
	}

	UE_LOG(LogAura, Display,
		TEXT("[GSM-Ready] Built context levelId=%s map=%s serverPort=%d gsm=%s:%d"),
		*OutLevelId,
		*CurrentMapPath,
		OutServerPort,
		*OutGameServerAddress,
		OutGameServerPort);

	return true;
}

bool AAuraGameModeBase::SendDedicatedServerReadyToGameServer(const FString& GameServerAddress, int32 GameServerPort, const FString& LevelId, int32 ServerPort) const
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Socket subsystem unavailable."));
		return false;
	}

	bool bIsValidIp = false;
	TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
	Address->SetIp(*GameServerAddress, bIsValidIp);
	if (!bIsValidIp)
	{
		const FAddressInfoResult AddressInfo = SocketSubsystem->GetAddressInfo(*GameServerAddress, nullptr, EAddressInfoFlags::Default, NAME_None);
		if (AddressInfo.ReturnCode != SE_NO_ERROR || AddressInfo.Results.IsEmpty())
		{
			UE_LOG(LogAura, Error, TEXT("[GSM-Ready] DNS resolve failed for GSM host '%s' (code=%d)."), *GameServerAddress, AddressInfo.ReturnCode);
			return false;
		}
		Address = AddressInfo.Results[0].Address->Clone();
	}

	Address->SetPort(GameServerPort);

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("AuraDedicatedServerReadyNotify"), Address->GetProtocolType());
	if (!Socket)
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Failed to create socket."));
		return false;
	}

	auto DestroySocket = [&]()
	{
		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
	};

	Socket->SetNonBlocking(false);
	Socket->SetReuseAddr(true);

	if (!Socket->Connect(*Address))
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Connect to GSM failed (%s:%d)."), *GameServerAddress, GameServerPort);
		DestroySocket();
		return false;
	}

	const FString Payload = FString::Printf(
		TEXT("{\"action\":\"server_ready\",\"levelId\":\"%s\",\"port\":%d,\"serverAuthToken\":\"%s\",\"readyNonce\":\"%s\"}\n"),
		*AuraGameModeBaseInternal::JsonEscape(LevelId),
		ServerPort,
		*AuraGameModeBaseInternal::JsonEscape(AuraGameModeBaseInternal::GetGsmServerAuthToken()),
		*AuraGameModeBaseInternal::JsonEscape(AuraGameModeBaseInternal::GetGsmReadyNonce()));

	FTCHARToUTF8 PayloadUtf8(*Payload);
	int32 BytesSent = 0;
	if (!Socket->Send(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length(), BytesSent) || BytesSent != PayloadUtf8.Length())
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Failed sending ready payload to GSM."));
		DestroySocket();
		return false;
	}

	FString ResponseLine;
	uint8 Byte = 0;
	int32 BytesRead = 0;
	const FTimespan WaitTimeout = FTimespan::FromSeconds(3.0);

	while (ResponseLine.Len() < 4096)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTimeout))
		{
			UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Timed out waiting for GSM ack."));
			DestroySocket();
			return false;
		}

		if (!Socket->Recv(&Byte, 1, BytesRead, ESocketReceiveFlags::None) || BytesRead == 0)
		{
			break;
		}

		if (Byte == '\n')
		{
			break;
		}

		ResponseLine.AppendChar(static_cast<TCHAR>(Byte));
	}

	DestroySocket();

	if (ResponseLine.IsEmpty())
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Empty ack response from GSM."));
		return false;
	}

	TSharedPtr<FJsonObject> AckObject;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseLine), AckObject) || !AckObject.IsValid())
	{
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] Invalid ack JSON from GSM: %s"), *ResponseLine);
		return false;
	}

	FString Status;
	AckObject->TryGetStringField(TEXT("status"), Status);
	if (!Status.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		FString Message;
		AckObject->TryGetStringField(TEXT("message"), Message);
		UE_LOG(LogAura, Error, TEXT("[GSM-Ready] GSM rejected ready notify: status=%s message=%s"), *Status, *Message);
		return false;
	}

	UE_LOG(LogAura, Display, TEXT("[GSM-Ready] GSM acknowledged server readiness. levelId=%s port=%d"), *LevelId, ServerPort);
	return true;
}

bool AAuraGameModeBase::LoadMonsterSpawnTable()
{
	LoadedMonsterSpawnRows.Reset();
	SpawnedMonsterRows.Reset();
	bMonsterSpawnTableLoaded = false;

	const TArray<FString> CandidatePaths = BuildCandidateMonsterSpawnTablePaths();
	FString JsonContent;
	FString LoadedFromPath;

	for (const FString& CandidatePath : CandidatePaths)
	{
		if (FPaths::FileExists(CandidatePath) && FFileHelper::LoadFileToString(JsonContent, *CandidatePath))
		{
			LoadedFromPath = CandidatePath;
			break;
		}
	}

	if (LoadedFromPath.IsEmpty())
	{
		UE_LOG(LogAura, Warning, TEXT("Monster spawn table not found. Expected file '%s' in Content/Config, Config, or Saved/Config."), *MonsterSpawnTableFileName);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogAura, Error, TEXT("Failed to parse monster spawn table JSON from %s"), *LoadedFromPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SpawnRowsJson = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("spawns"), SpawnRowsJson) && !RootObject->TryGetArrayField(TEXT("rows"), SpawnRowsJson))
	{
		UE_LOG(LogAura, Warning, TEXT("Monster spawn table at %s does not contain a 'spawns' array."), *LoadedFromPath);
		return false;
	}

	int32 ValidRows = 0;
	for (int32 RowIndex = 0; RowIndex < SpawnRowsJson->Num(); ++RowIndex)
	{
		const TSharedPtr<FJsonObject> RowObject = (*SpawnRowsJson)[RowIndex].IsValid() ? (*SpawnRowsJson)[RowIndex]->AsObject() : nullptr;
		if (!RowObject.IsValid())
		{
			UE_LOG(LogAura, Warning, TEXT("Monster spawn row %d is invalid (not an object)."), RowIndex);
			continue;
		}

		FMonsterSpawnTableRow Row;
		double RowId = static_cast<double>(Row.Id);
		if (RowObject->TryGetNumberField(TEXT("id"), RowId))
		{
			Row.Id = static_cast<int32>(RowId);
		}
		RowObject->TryGetStringField(TEXT("mapName"), Row.MapName);
		if (Row.MapName.IsEmpty())
		{
			RowObject->TryGetStringField(TEXT("map"), Row.MapName);
		}

		if (!RowObject->TryGetStringField(TEXT("monsterClassPath"), Row.MonsterClassPath))
		{
			RowObject->TryGetStringField(TEXT("classPath"), Row.MonsterClassPath);
		}

		double RowLevel = static_cast<double>(Row.Level);
		if (RowObject->TryGetNumberField(TEXT("level"), RowLevel))
		{
			Row.Level = FMath::Max(1, static_cast<int32>(RowLevel));
		}

		FString CharacterClassString;
		if (RowObject->TryGetStringField(TEXT("characterClass"), CharacterClassString))
		{
			if (!TryParseCharacterClass(CharacterClassString, Row.CharacterClass))
			{
				UE_LOG(LogAura, Warning, TEXT("Monster spawn row %d has invalid characterClass '%s'. Defaulting to Warrior."), RowIndex, *CharacterClassString);
				Row.CharacterClass = ECharacterClass::Warrior;
			}
		}

		RowObject->TryGetBoolField(TEXT("spawnOnLoad"), Row.bSpawnOnLoad);

		double RespawnTime = static_cast<double>(Row.RespawnTime);
		if (RowObject->TryGetNumberField(TEXT("respawnTime"), RespawnTime))
		{
			Row.RespawnTime = FMath::Max(0.f, static_cast<float>(RespawnTime));
		}

		const TSharedPtr<FJsonObject>* TransformObject = nullptr;
		if (RowObject->TryGetObjectField(TEXT("transform"), TransformObject) && TransformObject != nullptr && TransformObject->IsValid())
		{
			const TSharedPtr<FJsonObject>& Transform = *TransformObject;

			const TSharedPtr<FJsonObject>* LocationObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject != nullptr && LocationObject->IsValid())
			{
				double X = 0.0;
				double Y = 0.0;
				double Z = 0.0;
				(*LocationObject)->TryGetNumberField(TEXT("x"), X);
				(*LocationObject)->TryGetNumberField(TEXT("y"), Y);
				(*LocationObject)->TryGetNumberField(TEXT("z"), Z);
				Row.Transform.Location = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			}

			const TSharedPtr<FJsonObject>* RotationObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("rotation"), RotationObject) && RotationObject != nullptr && RotationObject->IsValid())
			{
				double Pitch = 0.0;
				double Yaw = 0.0;
				double Roll = 0.0;
				(*RotationObject)->TryGetNumberField(TEXT("pitch"), Pitch);
				(*RotationObject)->TryGetNumberField(TEXT("yaw"), Yaw);
				(*RotationObject)->TryGetNumberField(TEXT("roll"), Roll);
				Row.Transform.Rotation = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));
			}

			const TSharedPtr<FJsonObject>* ScaleObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("scale"), ScaleObject) && ScaleObject != nullptr && ScaleObject->IsValid())
			{
				double X = 1.0;
				double Y = 1.0;
				double Z = 1.0;
				(*ScaleObject)->TryGetNumberField(TEXT("x"), X);
				(*ScaleObject)->TryGetNumberField(TEXT("y"), Y);
				(*ScaleObject)->TryGetNumberField(TEXT("z"), Z);
				Row.Transform.Scale = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			}
		}

		if (Row.MonsterClassPath.IsEmpty())
		{
			UE_LOG(LogAura, Warning, TEXT("Monster spawn row %d is missing monsterClassPath. Skipping."), RowIndex);
			continue;
		}

		LoadedMonsterSpawnRows.Add(Row);
		++ValidRows;
	}

	bMonsterSpawnTableLoaded = ValidRows > 0;
	UE_LOG(LogAura, Display, TEXT("Loaded monster spawn table from %s with %d valid rows."), *LoadedFromPath, ValidRows);
	return bMonsterSpawnTableLoaded;
}

int32 AAuraGameModeBase::SpawnMonstersFromLoadedTable()
{
	if (!bMonsterSpawnTableLoaded)
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnMonstersFromLoadedTable called before table was loaded."));
		return 0;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogAura, Error, TEXT("SpawnMonstersFromLoadedTable failed because world is invalid."));
		return 0;
	}

	FString CurrentMapName = World->GetMapName();
	CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

	int32 SpawnedCount = 0;
	for (const FMonsterSpawnTableRow& Row : LoadedMonsterSpawnRows)
	{
		if (!Row.bSpawnOnLoad)
		{
			continue;
		}

		if (!ShouldSpawnRowForCurrentMap(Row, CurrentMapName))
		{
			continue;
		}

		if (SpawnMonsterFromRow(Row) != nullptr)
		{
			++SpawnedCount;
		}
	}

	return SpawnedCount;
}

AAuraEnemy* AAuraGameModeBase::SpawnMonsterByIdAtLocation(int32 MonsterId, const FVector& SpawnLocation)
{
	if (!HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnMonsterByIdAtLocation called without authority for monster id %d."), MonsterId);
		return nullptr;
	}

	if (!bMonsterSpawnTableLoaded && !LoadMonsterSpawnTable())
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnMonsterByIdAtLocation failed because the monster spawn table could not be loaded."));
		return nullptr;
	}

	const FMonsterSpawnTableRow* MonsterRow = LoadedMonsterSpawnRows.FindByPredicate(
		[MonsterId](const FMonsterSpawnTableRow& Row)
		{
			return Row.Id == MonsterId;
		});
	if (MonsterRow == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnMonsterByIdAtLocation could not find monster id %d."), MonsterId);
		return nullptr;
	}

	FMonsterSpawnTableRow SpawnRow = *MonsterRow;
	SpawnRow.Transform.Location = SpawnLocation;
	return SpawnMonsterFromRow(SpawnRow);
}

bool AAuraGameModeBase::ShouldSpawnRowForCurrentMap(const FMonsterSpawnTableRow& Row, const FString& CurrentMapName) const
{
	if (Row.MapName.IsEmpty())
	{
		return true;
	}

	return Row.MapName.Equals(CurrentMapName, ESearchCase::IgnoreCase);
}

AAuraEnemy* AAuraGameModeBase::SpawnMonsterFromRow(const FMonsterSpawnTableRow& Row)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	const TSubclassOf<AAuraEnemy> EnemyClass = ResolveMonsterClassFromPath(Row.MonsterClassPath);
	if (!EnemyClass)
	{
		UE_LOG(LogAura, Warning, TEXT("Failed to load monster class '%s' for row '%d'."), *Row.MonsterClassPath, Row.Id);
		return nullptr;
	}

	const FTransform SpawnTransform(Row.Transform.Rotation, Row.Transform.Location, Row.Transform.Scale);
	AAuraEnemy* Enemy = World->SpawnActorDeferred<AAuraEnemy>(EnemyClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!IsValid(Enemy))
	{
		UE_LOG(LogAura, Warning, TEXT("Failed to spawn enemy for row '%d'."), Row.Id);
		return nullptr;
	}

	Enemy->SetLevel(Row.Level);
	Enemy->SetCharacterClass(Row.CharacterClass);
	Enemy->FinishSpawning(SpawnTransform);
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Spawn] Spawned row=%d enemy=%s class=%s location=%s"),
		Row.Id,
		*GetNameSafe(Enemy),
		*GetNameSafe(EnemyClass),
		*SpawnTransform.GetLocation().ToCompactString());

	Enemy->SpawnDefaultController();
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Spawn] Controller after SpawnDefaultController: row=%d enemy=%s controller=%s"),
		Row.Id,
		*GetNameSafe(Enemy),
		*GetNameSafe(Enemy->GetController()));

	if (Row.RespawnTime > 0.f)
	{
		SpawnedMonsterRows.Add(TWeakObjectPtr<AActor>(Enemy), Row);
		Enemy->OnDestroyed.AddDynamic(this, &AAuraGameModeBase::OnSpawnedMonsterDestroyed);
	}

	return Enemy;
}

void AAuraGameModeBase::OnSpawnedMonsterDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsValid(DestroyedActor))
	{
		return;
	}

	FMonsterSpawnTableRow SpawnRow;
	if (!SpawnedMonsterRows.RemoveAndCopyValue(TWeakObjectPtr<AActor>(DestroyedActor), SpawnRow))
	{
		return;
	}

	if (SpawnRow.RespawnTime <= 0.f)
	{
		return;
	}

	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindLambda([this, SpawnRow]()
	{
		if (!HasAuthority())
		{
			return;
		}

		UWorld* World = GetWorld();
		if (!IsValid(World) || World->bIsTearingDown)
		{
			return;
		}

		FString CurrentMapName = World->GetMapName();
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
		if (!ShouldSpawnRowForCurrentMap(SpawnRow, CurrentMapName))
		{
			return;
		}

		SpawnMonsterFromRow(SpawnRow);
	});

	FTimerHandle RespawnTimerHandle;
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, SpawnRow.RespawnTime, false);
}

bool AAuraGameModeBase::TryParseCharacterClass(const FString& InValue, ECharacterClass& OutCharacterClass)
{
	const FString Trimmed = InValue.TrimStartAndEnd();
	if (Trimmed.Equals(TEXT("Elementalist"), ESearchCase::IgnoreCase))
	{
		OutCharacterClass = ECharacterClass::Elementalist;
		return true;
	}

	if (Trimmed.Equals(TEXT("Warrior"), ESearchCase::IgnoreCase))
	{
		OutCharacterClass = ECharacterClass::Warrior;
		return true;
	}

	if (Trimmed.Equals(TEXT("Ranger"), ESearchCase::IgnoreCase))
	{
		OutCharacterClass = ECharacterClass::Ranger;
		return true;
	}

	return false;
}

TSubclassOf<AAuraEnemy> AAuraGameModeBase::ResolveMonsterClassFromPath(const FString& ClassPath) const
{
	if (ClassPath.IsEmpty())
	{
		return nullptr;
	}

	FString NormalizedPath = ClassPath;
	if (ClassPath.StartsWith(TEXT("/Game/")) && !ClassPath.EndsWith(TEXT("_C")))
	{
		const FString AssetName = FPackageName::GetShortName(ClassPath);
		NormalizedPath = FString::Printf(TEXT("%s.%s_C"), *ClassPath, *AssetName);
	}

	TSoftClassPtr<AAuraEnemy> SoftClass{FSoftObjectPath(NormalizedPath)};
	UClass* LoadedClass = SoftClass.LoadSynchronous();
	if (!LoadedClass)
	{
		LoadedClass = StaticLoadClass(AAuraEnemy::StaticClass(), nullptr, *NormalizedPath);
	}

	if (LoadedClass && LoadedClass->IsChildOf(AAuraEnemy::StaticClass()))
	{
		return LoadedClass;
	}

	return nullptr;
}

TArray<FString> AAuraGameModeBase::BuildCandidateMonsterSpawnTablePaths() const
{
	TArray<FString> CandidatePaths;
	CandidatePaths.Reserve(3);

	// Search order: runtime override (Saved/Config) -> canonical cook-safe location
	// (Content/Config, packaged with the build) -> legacy project Config/ fallback
	// (still staged via DirectoriesToAlwaysStageAsUFS). Content/Config is the source of
	// truth; the others are override/back-compat paths. Matches RoleConfig/LevelConfig.
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), MonsterSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), MonsterSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectConfigDir(), MonsterSpawnTableFileName));

	return CandidatePaths;
}

bool AAuraGameModeBase::LoadItemSpawnTable()
{
	LoadedItemSpawnRows.Reset();
	SpawnedItemRows.Reset();
	bItemSpawnTableLoaded = false;

	const TArray<FString> CandidatePaths = BuildCandidateItemSpawnTablePaths();
	FString JsonContent;
	FString LoadedFromPath;

	for (const FString& CandidatePath : CandidatePaths)
	{
		if (FPaths::FileExists(CandidatePath) && FFileHelper::LoadFileToString(JsonContent, *CandidatePath))
		{
			LoadedFromPath = CandidatePath;
			break;
		}
	}

	if (LoadedFromPath.IsEmpty())
	{
		UE_LOG(LogAura, Warning, TEXT("Item spawn table not found. Expected file '%s' in Content/Config, Config, or Saved/Config."), *ItemSpawnTableFileName);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogAura, Error, TEXT("Failed to parse item spawn table JSON from %s"), *LoadedFromPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SpawnRowsJson = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("spawns"), SpawnRowsJson) && !RootObject->TryGetArrayField(TEXT("rows"), SpawnRowsJson))
	{
		UE_LOG(LogAura, Warning, TEXT("Item spawn table at %s does not contain a 'spawns' array."), *LoadedFromPath);
		return false;
	}

	int32 ValidRows = 0;
	for (int32 RowIndex = 0; RowIndex < SpawnRowsJson->Num(); ++RowIndex)
	{
		const TSharedPtr<FJsonObject> RowObject = (*SpawnRowsJson)[RowIndex].IsValid() ? (*SpawnRowsJson)[RowIndex]->AsObject() : nullptr;
		if (!RowObject.IsValid())
		{
			UE_LOG(LogAura, Warning, TEXT("Item spawn row %d is invalid (not an object)."), RowIndex);
			continue;
		}

		FItemSpawnTableRow Row;
		RowObject->TryGetStringField(TEXT("id"), Row.Id);
		RowObject->TryGetStringField(TEXT("itemKind"), Row.ItemKind);
		if (Row.ItemKind.IsEmpty())
		{
			if (!RowObject->TryGetStringField(TEXT("itemType"), Row.ItemKind))
			{
				RowObject->TryGetStringField(TEXT("type"), Row.ItemKind);
			}
		}

		RowObject->TryGetStringField(TEXT("mapName"), Row.MapName);
		if (Row.MapName.IsEmpty())
		{
			RowObject->TryGetStringField(TEXT("map"), Row.MapName);
		}

		if (!RowObject->TryGetStringField(TEXT("itemClassPath"), Row.ItemClassPath))
		{
			RowObject->TryGetStringField(TEXT("classPath"), Row.ItemClassPath);
		}

		if (!RowObject->TryGetStringField(TEXT("destinationServerId"), Row.DestinationServerId))
		{
			RowObject->TryGetStringField(TEXT("destinationServer"), Row.DestinationServerId);
		}

		RowObject->TryGetBoolField(TEXT("spawnOnLoad"), Row.bSpawnOnLoad);

		double RespawnTime = static_cast<double>(Row.RespawnTime);
		if (RowObject->TryGetNumberField(TEXT("respawnTime"), RespawnTime))
		{
			Row.RespawnTime = FMath::Max(0.f, static_cast<float>(RespawnTime));
		}

		const TSharedPtr<FJsonObject>* TransformObject = nullptr;
		if (RowObject->TryGetObjectField(TEXT("transform"), TransformObject) && TransformObject != nullptr && TransformObject->IsValid())
		{
			const TSharedPtr<FJsonObject>& Transform = *TransformObject;

			const TSharedPtr<FJsonObject>* LocationObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject != nullptr && LocationObject->IsValid())
			{
				double X = 0.0;
				double Y = 0.0;
				double Z = 0.0;
				(*LocationObject)->TryGetNumberField(TEXT("x"), X);
				(*LocationObject)->TryGetNumberField(TEXT("y"), Y);
				(*LocationObject)->TryGetNumberField(TEXT("z"), Z);
				Row.Transform.Location = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			}

			const TSharedPtr<FJsonObject>* RotationObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("rotation"), RotationObject) && RotationObject != nullptr && RotationObject->IsValid())
			{
				double Pitch = 0.0;
				double Yaw = 0.0;
				double Roll = 0.0;
				(*RotationObject)->TryGetNumberField(TEXT("pitch"), Pitch);
				(*RotationObject)->TryGetNumberField(TEXT("yaw"), Yaw);
				(*RotationObject)->TryGetNumberField(TEXT("roll"), Roll);
				Row.Transform.Rotation = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));
			}

			const TSharedPtr<FJsonObject>* ScaleObject = nullptr;
			if (Transform->TryGetObjectField(TEXT("scale"), ScaleObject) && ScaleObject != nullptr && ScaleObject->IsValid())
			{
				double X = 1.0;
				double Y = 1.0;
				double Z = 1.0;
				(*ScaleObject)->TryGetNumberField(TEXT("x"), X);
				(*ScaleObject)->TryGetNumberField(TEXT("y"), Y);
				(*ScaleObject)->TryGetNumberField(TEXT("z"), Z);
				Row.Transform.Scale = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			}
		}

		if (Row.ItemClassPath.IsEmpty())
		{
			UE_LOG(LogAura, Warning, TEXT("Item spawn row %d is missing itemClassPath. Skipping."), RowIndex);
			continue;
		}

		LoadedItemSpawnRows.Add(Row);
		++ValidRows;
	}

	bItemSpawnTableLoaded = ValidRows > 0;
	UE_LOG(LogAura, Display, TEXT("Loaded item spawn table from %s with %d valid rows."), *LoadedFromPath, ValidRows);
	return bItemSpawnTableLoaded;
}

int32 AAuraGameModeBase::SpawnItemsFromLoadedTable()
{
	if (!HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnItemsFromLoadedTable called without authority. Ignoring."));
		return 0;
	}

	if (!bItemSpawnTableLoaded)
	{
		UE_LOG(LogAura, Warning, TEXT("SpawnItemsFromLoadedTable called before table was loaded."));
		return 0;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogAura, Error, TEXT("SpawnItemsFromLoadedTable failed because world is invalid."));
		return 0;
	}

	FString CurrentMapName = World->GetMapName();
	CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

	int32 SpawnedCount = 0;
	for (const FItemSpawnTableRow& Row : LoadedItemSpawnRows)
	{
		if (!Row.bSpawnOnLoad)
		{
			UE_LOG(LogAura, Verbose, TEXT("[ItemSpawn] Skipping row=%s because spawnOnLoad=false"), *Row.Id);
			continue;
		}

		if (!ShouldSpawnItemRowForCurrentMap(Row, CurrentMapName))
		{
			UE_LOG(LogAura, Verbose, TEXT("[ItemSpawn] Skipping row=%s because map mismatch (row=%s current=%s)"),
				*Row.Id,
				*Row.MapName,
				*CurrentMapName);
			continue;
		}

		if (SpawnItemFromRow(Row) != nullptr)
		{
			++SpawnedCount;
		}
	}

	UE_LOG(LogAura, Display, TEXT("[ItemSpawn] SpawnItemsFromLoadedTable completed. Spawned=%d Considered=%d Map=%s"),
		SpawnedCount,
		LoadedItemSpawnRows.Num(),
		*CurrentMapName);

	return SpawnedCount;
}

bool AAuraGameModeBase::ShouldSpawnItemRowForCurrentMap(const FItemSpawnTableRow& Row, const FString& CurrentMapName) const
{
	if (Row.MapName.IsEmpty())
	{
		return true;
	}

	return Row.MapName.Equals(CurrentMapName, ESearchCase::IgnoreCase);
}

AActor* AAuraGameModeBase::SpawnItemFromRow(const FItemSpawnTableRow& Row)
{
	if (!HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[ItemSpawn] SpawnItemFromRow called without authority for row=%s"), *Row.Id);
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogAura, Error, TEXT("[ItemSpawn] World invalid when spawning row=%s"), *Row.Id);
		return nullptr;
	}

	const TSubclassOf<AActor> ItemClass = ResolveItemClassFromPath(Row.ItemClassPath);
	if (!ItemClass)
	{
		UE_LOG(LogAura, Warning, TEXT("Failed to load item class '%s' for row '%s'."), *Row.ItemClassPath, *Row.Id);
		return nullptr;
	}

	const AActor* ItemClassCDO = ItemClass->GetDefaultObject<AActor>();
	const bool bClassReplicates = IsValid(ItemClassCDO) ? ItemClassCDO->GetIsReplicated() : false;
	const bool bClassReplicateMovement = IsValid(ItemClassCDO) ? ItemClassCDO->IsReplicatingMovement() : false;

	const FTransform SpawnTransform(Row.Transform.Rotation, Row.Transform.Location, Row.Transform.Scale);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* SpawnedItem = World->SpawnActor<AActor>(ItemClass, SpawnTransform, SpawnParams);

	if (!IsValid(SpawnedItem))
	{
		UE_LOG(LogAura, Warning, TEXT("Failed to spawn item for row '%s'."), *Row.Id);
		return nullptr;
	}

	if (!SpawnedItem->GetIsReplicated())
	{
		SpawnedItem->SetReplicates(true);
		UE_LOG(LogAura, Warning, TEXT("[ItemSpawn] row=%s class=%s was non-replicated at spawn; forcing SetReplicates(true). Check BP class defaults."),
			*Row.Id,
			*GetNameSafe(ItemClass));
	}

	if (bClassReplicateMovement)
	{
		SpawnedItem->SetReplicateMovement(true);
	}

	if (ALevelJumpPortal* JumpPortal = Cast<ALevelJumpPortal>(SpawnedItem))
	{
		JumpPortal->DestinationServerId = Row.DestinationServerId;
	}

	UE_LOG(LogAura, Display, TEXT("[ItemSpawn] Spawned row=%s kind=%s actor=%s class=%s location=%s destinationServerId=%s classReplicates=%s classRepMove=%s actorReplicates=%s actorRepMove=%s role=%d"),
		*Row.Id,
		*Row.ItemKind,
		*GetNameSafe(SpawnedItem),
		*GetNameSafe(ItemClass),
		*SpawnTransform.GetLocation().ToCompactString(),
		*Row.DestinationServerId,
		bClassReplicates ? TEXT("true") : TEXT("false"),
		bClassReplicateMovement ? TEXT("true") : TEXT("false"),
		SpawnedItem->GetIsReplicated() ? TEXT("true") : TEXT("false"),
		SpawnedItem->IsReplicatingMovement() ? TEXT("true") : TEXT("false"),
		(int32)SpawnedItem->GetLocalRole());

	if (Row.RespawnTime > 0.f)
	{
		SpawnedItemRows.Add(TWeakObjectPtr<AActor>(SpawnedItem), Row);
		SpawnedItem->OnDestroyed.AddDynamic(this, &AAuraGameModeBase::OnSpawnedItemDestroyed);
	}

	return SpawnedItem;
}

void AAuraGameModeBase::OnSpawnedItemDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !IsValid(DestroyedActor))
	{
		return;
	}

	FItemSpawnTableRow SpawnRow;
	if (!SpawnedItemRows.RemoveAndCopyValue(TWeakObjectPtr<AActor>(DestroyedActor), SpawnRow))
	{
		return;
	}

	if (SpawnRow.RespawnTime <= 0.f)
	{
		return;
	}

	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindLambda([this, SpawnRow]()
	{
		if (!HasAuthority())
		{
			return;
		}

		UWorld* World = GetWorld();
		if (!IsValid(World) || World->bIsTearingDown)
		{
			return;
		}

		FString CurrentMapName = World->GetMapName();
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
		if (!ShouldSpawnItemRowForCurrentMap(SpawnRow, CurrentMapName))
		{
			return;
		}

		SpawnItemFromRow(SpawnRow);
	});

	FTimerHandle RespawnTimerHandle;
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, SpawnRow.RespawnTime, false);
}

TSubclassOf<AActor> AAuraGameModeBase::ResolveItemClassFromPath(const FString& ClassPath) const
{
	if (ClassPath.IsEmpty())
	{
		return nullptr;
	}

	FString NormalizedPath = ClassPath;
	if (ClassPath.StartsWith(TEXT("/Game/")) && !ClassPath.EndsWith(TEXT("_C")))
	{
		const FString AssetName = FPackageName::GetShortName(ClassPath);
		NormalizedPath = FString::Printf(TEXT("%s.%s_C"), *ClassPath, *AssetName);
	}

	TSoftClassPtr<AActor> SoftClass{FSoftObjectPath(NormalizedPath)};
	UClass* LoadedClass = SoftClass.LoadSynchronous();
	if (!LoadedClass)
	{
		LoadedClass = StaticLoadClass(AActor::StaticClass(), nullptr, *NormalizedPath);
	}

	if (LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass()))
	{
		return LoadedClass;
	}

	return nullptr;
}

TArray<FString> AAuraGameModeBase::BuildCandidateItemSpawnTablePaths() const
{
	TArray<FString> CandidatePaths;
	CandidatePaths.Reserve(3);

	// Search order: runtime override (Saved/Config) -> canonical cook-safe location
	// (Content/Config, packaged with the build) -> legacy project Config/ fallback
	// (still staged via DirectoriesToAlwaysStageAsUFS). Content/Config is the source of
	// truth; the others are override/back-compat paths. Matches RoleConfig/LevelConfig.
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), ItemSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), ItemSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectConfigDir(), ItemSpawnTableFileName));

	return CandidatePaths;
}
