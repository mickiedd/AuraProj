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
class URuntimeAbilityInfo;
class UCharacterClassInfo;
class URoleInfo;
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
	int32 Id = 0;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster Spawn")
	float RespawnTime = 0.f;
};

USTRUCT(BlueprintType)
struct FItemSpawnTransformData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct FItemSpawnTableRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FString ItemKind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FString ItemClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FString DestinationServerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	FItemSpawnTransformData Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	bool bSpawnOnLoad = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	float RespawnTime = 0.f;
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

	/** Per-role config. Built at BeginPlay from Content/Config/RoleConfig.json (replaces DA_RoleInfo). */
	UPROPERTY(Transient)
	TObjectPtr<URoleInfo> RoleInfo;

	/**
	 * How often (seconds) the server polls Content/Config/.RoleConfig.reload for the editor
	 * "Reload Role Config" mending tool. When the sentinel is found, RoleInfo is rebuilt from
	 * RoleConfig.json so new logins/spawns use the new defaultRole. <= 0 disables the poll.
	 * Clients are unaffected (they poll inside UAuraAbilitySystemLibrary::GetRoleInfo).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Role Config", meta = (ClampMin = "0.0"))
	float RoleConfigPollInterval = 1.0f;

	/** Runtime ability UI metadata. Built on-demand from Content/Config/AbilityInfo.json (replaces DA_AbilityInfo UAsset). */
	UPROPERTY(Transient)
	TObjectPtr<URuntimeAbilityInfo> RuntimeAbilityInfo;

	UPROPERTY(EditDefaultsOnly, Category = "Loot Tiers")
	TObjectPtr<ULootTiers> LootTiers;

	UPROPERTY(EditDefaultsOnly, Category = "Monster Spawn")
	bool bEnableMonsterTableAutoSpawn = true;

	UPROPERTY(EditDefaultsOnly, Category = "Monster Spawn")
	FString MonsterSpawnTableFileName = TEXT("MonsterSpawnTable.json");

	UPROPERTY(EditDefaultsOnly, Category = "Item Spawn")
	bool bEnableItemTableAutoSpawn = false;

	UPROPERTY(EditDefaultsOnly, Category = "Item Spawn")
	FString ItemSpawnTableFileName = TEXT("ItemSpawnTable.json");

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	bool bEnablePlayerRespawn = true;

	UPROPERTY(EditDefaultsOnly)
	TMap<FString, TSoftObjectPtr<UWorld>> Maps;

	FString GetMapNameFromMapAssetName(const FString& MapAssetName) const;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void PlayerDied(ACharacter* DeadCharacter, float RespawnDelay);

	/** Spawn a loaded monster-table entry at a caller-provided location. */
	AAuraEnemy* SpawnMonsterByIdAtLocation(int32 MonsterId, const FVector& SpawnLocation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * On dedicated server, notify GSM that this map is fully loaded and accepting clients.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Dedicated Server|GSM")
	bool bNotifyGameServerManagerWhenReady = true;

	UPROPERTY(EditDefaultsOnly, Category = "Dedicated Server|GSM", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float GameServerReadyNotifyInitialDelaySeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Dedicated Server|GSM", meta = (ClampMin = "0.5", ClampMax = "30.0"))
	float GameServerReadyNotifyRetryIntervalSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Dedicated Server|GSM", meta = (ClampMin = "1", ClampMax = "60"))
	int32 GameServerReadyNotifyMaxAttempts = 10;

	UFUNCTION(BlueprintCallable, Category = "Monster Spawn")
	bool LoadMonsterSpawnTable();

	UFUNCTION(BlueprintCallable, Category = "Monster Spawn")
	int32 SpawnMonstersFromLoadedTable();

	UFUNCTION(BlueprintPure, Category = "Monster Spawn")
	const TArray<FMonsterSpawnTableRow>& GetLoadedMonsterSpawnRows() const { return LoadedMonsterSpawnRows; }

	UFUNCTION(BlueprintCallable, Category = "Item Spawn")
	bool LoadItemSpawnTable();

	UFUNCTION(BlueprintCallable, Category = "Item Spawn")
	int32 SpawnItemsFromLoadedTable();

	UFUNCTION(BlueprintPure, Category = "Item Spawn")
	const TArray<FItemSpawnTableRow>& GetLoadedItemSpawnRows() const { return LoadedItemSpawnRows; }

private:
	FString BuildUniquePlayerName(const FString& RequestedName, const FString& DisambiguationToken = FString(), const APlayerState* ExcludedPlayerState = nullptr) const;
	FString BuildConnectionDisambiguationToken(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId) const;
	bool IsPlayerNameInUse(const FString& CandidateName, const APlayerState* ExcludedPlayerState = nullptr) const;
	static FString SanitizePlayerName(const FString& RawName);
	FName GetActivePlayerStartTag() const;
	void RespawnPlayer(AController* DeadController);
	void ReloadMapAfterPlayerDeath();
	bool ShouldSpawnRowForCurrentMap(const FMonsterSpawnTableRow& Row, const FString& CurrentMapName) const;
	AAuraEnemy* SpawnMonsterFromRow(const FMonsterSpawnTableRow& Row);

	UFUNCTION()
	void OnSpawnedMonsterDestroyed(AActor* DestroyedActor);
	bool ShouldSpawnItemRowForCurrentMap(const FItemSpawnTableRow& Row, const FString& CurrentMapName) const;
	AActor* SpawnItemFromRow(const FItemSpawnTableRow& Row);

	UFUNCTION()
	void OnSpawnedItemDestroyed(AActor* DestroyedActor);

	void HandleDedicatedServerReadyNotify();
	bool TryBuildDedicatedServerReadyContext(FString& OutLevelId, int32& OutServerPort, FString& OutGameServerAddress, int32& OutGameServerPort) const;
	bool SendDedicatedServerReadyToGameServer(const FString& GameServerAddress, int32 GameServerPort, const FString& LevelId, int32 ServerPort) const;

	static bool TryParseCharacterClass(const FString& InValue, ECharacterClass& OutCharacterClass);
	TSubclassOf<AAuraEnemy> ResolveMonsterClassFromPath(const FString& ClassPath) const;
	TSubclassOf<AActor> ResolveItemClassFromPath(const FString& ClassPath) const;
	TArray<FString> BuildCandidateMonsterSpawnTablePaths() const;
	TArray<FString> BuildCandidateItemSpawnTablePaths() const;

	UPROPERTY(VisibleAnywhere, Category = "Monster Spawn")
	TArray<FMonsterSpawnTableRow> LoadedMonsterSpawnRows;

	UPROPERTY(VisibleAnywhere, Category = "Item Spawn")
	TArray<FItemSpawnTableRow> LoadedItemSpawnRows;

	TMap<TWeakObjectPtr<AActor>, FMonsterSpawnTableRow> SpawnedMonsterRows;
	TMap<TWeakObjectPtr<AActor>, FItemSpawnTableRow> SpawnedItemRows;

	bool bMonsterSpawnTableLoaded = false;
	bool bItemSpawnTableLoaded = false;
	int32 DedicatedServerReadyNotifyAttempts = 0;
	FTimerHandle DedicatedServerReadyNotifyTimerHandle;

	FTimerHandle RoleConfigPollTimerHandle;

};
