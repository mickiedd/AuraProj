// Copyright Druid Mechanics


#include "Game/AuraGameModeBase.h"

#include "EngineUtils.h"
#include "Aura/AuraLogChannels.h"
#include "Game/AuraGameInstance.h"
#include "Game/LoadScreenSaveGame.h"
#include "GameFramework/PlayerStart.h"
#include "Interaction/SaveInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UI/ViewModel/MVVM_LoadSlot.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/NetConnection.h"
#include "Character/AuraEnemy.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

	UGameplayStatics::OpenLevelBySoftObjectPtr(Slot, Maps.FindChecked(Slot->GetMapName()));
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

FString AAuraGameModeBase::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
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

	UE_LOG(LogTemp, Display, TEXT("Assigned player name '%s' (requested: '%s', token: '%s')"), *UniqueName, *RequestedName, *DisambiguationToken);

	return Result;
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
		AActor* SelectedActor = Actors[0];
		for (AActor* Actor : Actors)
		{
			if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
			{
				if (PlayerStart->PlayerStartTag == DesiredPlayerStartTag)
				{
					SelectedActor = PlayerStart;
					break;
				}
			}
		}
		return SelectedActor;
	}
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

	UGameplayStatics::OpenLevel(this, FName(SaveGame->MapAssetName));
}

void AAuraGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	Maps.Add(DefaultMapName, DefaultMap);

	if (!bEnableMonsterTableAutoSpawn)
	{
		return;
	}

	if (LoadMonsterSpawnTable())
	{
		const int32 SpawnedCount = SpawnMonstersFromLoadedTable();
		UE_LOG(LogAura, Display, TEXT("Monster auto-spawn completed. Spawned %d monsters."), SpawnedCount);
	}
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
		UE_LOG(LogAura, Warning, TEXT("Monster spawn table not found. Expected file '%s' in Config, Data, or Saved/Config."), *MonsterSpawnTableFileName);
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
		RowObject->TryGetStringField(TEXT("id"), Row.Id);
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
		UE_LOG(LogAura, Warning, TEXT("Failed to load monster class '%s' for row '%s'."), *Row.MonsterClassPath, *Row.Id);
		return nullptr;
	}

	const FTransform SpawnTransform(Row.Transform.Rotation, Row.Transform.Location, Row.Transform.Scale);
	AAuraEnemy* Enemy = World->SpawnActorDeferred<AAuraEnemy>(EnemyClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!IsValid(Enemy))
	{
		UE_LOG(LogAura, Warning, TEXT("Failed to spawn enemy for row '%s'."), *Row.Id);
		return nullptr;
	}

	Enemy->SetLevel(Row.Level);
	Enemy->SetCharacterClass(Row.CharacterClass);
	Enemy->FinishSpawning(SpawnTransform);
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Spawn] Spawned row=%s enemy=%s class=%s location=%s"),
		*Row.Id,
		*GetNameSafe(Enemy),
		*GetNameSafe(EnemyClass),
		*SpawnTransform.GetLocation().ToCompactString());

	Enemy->SpawnDefaultController();
	UE_LOG(LogAura, Log, TEXT("[EnemyAI][Spawn] Controller after SpawnDefaultController: row=%s enemy=%s controller=%s"),
		*Row.Id,
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

	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), MonsterSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectConfigDir(), MonsterSpawnTableFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), MonsterSpawnTableFileName));

	return CandidatePaths;
}
