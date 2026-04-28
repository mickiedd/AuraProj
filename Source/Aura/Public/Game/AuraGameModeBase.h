// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "GameFramework/GameModeBase.h"
#include "AuraGameModeBase.generated.h"

class ULootTiers;
class ULoadScreenSaveGame;
class USaveGame;
class UMVVM_LoadSlot;
class UAbilityInfo;
class UCharacterClassInfo;
class AAuraEnemy;
class APlayerController;
class APlayerState;
struct FUniqueNetIdRepl;

USTRUCT(BlueprintType)
struct FMonsterSpawnTransformData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct FMonsterSpawnTableRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FString MonsterClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	ECharacterClass CharacterClass = ECharacterClass::Warrior;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	FMonsterSpawnTransformData Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	bool bSpawnOnLoad = true;
};
/**
 * 
 */
UCLASS()
class AURA_API AAuraGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;

	UPROPERTY(EditDefaultsOnly, Category = "Loot Tiers")
	TObjectPtr<ULootTiers> LootTiers;

	UPROPERTY(EditDefaultsOnly, Category = "Monster Spawn")
	bool bEnableMonsterTableAutoSpawn = true;

	UPROPERTY(EditDefaultsOnly, Category = "Monster Spawn")
	FString MonsterSpawnTableFileName = TEXT("MonsterSpawnTable.json");

	void SaveSlotData(UMVVM_LoadSlot* LoadSlot, int32 SlotIndex);
	ULoadScreenSaveGame* GetSaveSlotData(const FString& SlotName, int32 SlotIndex) const;
	static void DeleteSlot(const FString& SlotName, int32 SlotIndex);
	ULoadScreenSaveGame* RetrieveInGameSaveData();
	void SaveInGameProgressData(ULoadScreenSaveGame* SaveObject);

	void SaveWorldState(UWorld* World, const FString& DestinationMapAssetName = FString("")) const;
	void LoadWorldState(UWorld* World) const;

	void TravelToMap(UMVVM_LoadSlot* Slot);

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<USaveGame> LoadScreenSaveGameClass;

	UPROPERTY(EditDefaultsOnly)
	FString DefaultMapName;

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UWorld> DefaultMap;

	UPROPERTY(EditDefaultsOnly)
	FName DefaultPlayerStartTag;

	UPROPERTY(EditDefaultsOnly)
	TMap<FString, TSoftObjectPtr<UWorld>> Maps;

	FString GetMapNameFromMapAssetName(const FString& MapAssetName) const;

	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void PlayerDied(ACharacter* DeadCharacter);
protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Monster Spawn")
	bool LoadMonsterSpawnTable();

	UFUNCTION(BlueprintCallable, Category = "Monster Spawn")
	int32 SpawnMonstersFromLoadedTable();

	UFUNCTION(BlueprintPure, Category = "Monster Spawn")
	const TArray<FMonsterSpawnTableRow>& GetLoadedMonsterSpawnRows() const { return LoadedMonsterSpawnRows; }

private:
	FString BuildUniquePlayerName(const FString& RequestedName, const FString& DisambiguationToken = FString(), const APlayerState* ExcludedPlayerState = nullptr) const;
	FString BuildConnectionDisambiguationToken(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId) const;
	bool IsPlayerNameInUse(const FString& CandidateName, const APlayerState* ExcludedPlayerState = nullptr) const;
	static FString SanitizePlayerName(const FString& RawName);
	bool ShouldSpawnRowForCurrentMap(const FMonsterSpawnTableRow& Row, const FString& CurrentMapName) const;
	static bool TryParseCharacterClass(const FString& InValue, ECharacterClass& OutCharacterClass);
	TSubclassOf<AAuraEnemy> ResolveMonsterClassFromPath(const FString& ClassPath) const;
	TArray<FString> BuildCandidateMonsterSpawnTablePaths() const;

	UPROPERTY(VisibleAnywhere, Category = "Monster Spawn")
	TArray<FMonsterSpawnTableRow> LoadedMonsterSpawnRows;

	bool bMonsterSpawnTableLoaded = false;
	
};
