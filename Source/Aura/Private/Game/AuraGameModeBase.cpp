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

AActor* AAuraGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());
	
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Actors);
	if (Actors.Num() > 0)
	{
		AActor* SelectedActor = Actors[0];
		for (AActor* Actor : Actors)
		{
			if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
			{
				if (PlayerStart->PlayerStartTag == AuraGameInstance->PlayerStartTag)
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

void AAuraGameModeBase::PlayerDied(ACharacter* DeadCharacter)
{
	ULoadScreenSaveGame* SaveGame = RetrieveInGameSaveData();
	if (!IsValid(SaveGame)) return;

	UGameplayStatics::OpenLevel(DeadCharacter, FName(SaveGame->MapAssetName));
}

void AAuraGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	Maps.Add(DefaultMapName, DefaultMap);
}
