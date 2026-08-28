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
class UAuraPopulationManager;
class UAuraDeathPolicyDispatcher;
class UAuraEconomyRegistrySubsystem;
class UAuraPersistenceSubsystem;
class AAuraBattleDirector;
struct FAuraDeathEvent;
class AAuraEnemy;
class APlayerController;
class APlayerState;
class APawn;
struct FUniqueNetIdRepl;

UENUM(BlueprintType)
enum class EAuraWorldReadiness : uint8
{
	Initializing,
	Ready,
	Unhealthy,
};

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

	/** Authority-only Day 09 population owner, initialized before StartPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UAuraPopulationManager> PopulationManager;

	/** Process-lifetime immutable authority economy snapshot; published before population spawning. */
	UPROPERTY(Transient)
	TObjectPtr<UAuraEconomyRegistrySubsystem> EconomyRegistry;

	/** Authority-only profile/world persistence coordinator. */
	UPROPERTY(Transient)
	TObjectPtr<UAuraPersistenceSubsystem> PersistenceSubsystem;

	/** Server-wide neutral death event boundary; policy handlers bind once here. */
	UPROPERTY(Transient)
	TObjectPtr<UAuraDeathPolicyDispatcher> DeathPolicyDispatcher;

	/** Server-owned phase/event and battle-zone resolver. */
	UPROPERTY(Transient)
	TObjectPtr<AAuraBattleDirector> BattleDirector;

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
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void PlayerDied(ACharacter* DeadCharacter, float RespawnDelay);

	/** Spawn a loaded monster-table entry at a caller-provided location. */
	AAuraEnemy* SpawnMonsterByIdAtLocation(int32 MonsterId, const FVector& SpawnLocation);

	const UAuraPopulationManager* GetPopulationManager() const { return PopulationManager; }
	UAuraPopulationManager* GetPopulationManagerMutable() const { return PopulationManager; }
	const UAuraEconomyRegistrySubsystem* GetEconomyRegistry() const { return EconomyRegistry; }
	const UAuraPersistenceSubsystem* GetPersistenceSubsystem() const { return PersistenceSubsystem; }
	UAuraPersistenceSubsystem* GetPersistenceSubsystemMutable() const { return PersistenceSubsystem; }
	const UAuraDeathPolicyDispatcher* GetDeathPolicyDispatcher() const { return DeathPolicyDispatcher; }
	UAuraDeathPolicyDispatcher* GetDeathPolicyDispatcherMutable() const { return DeathPolicyDispatcher; }
	const AAuraBattleDirector* GetBattleDirector() const { return BattleDirector; }
	AAuraBattleDirector* GetBattleDirectorMutable() const { return BattleDirector; }
	EAuraWorldReadiness GetWorldReadiness() const { return WorldReadiness; }
	const FString& GetWorldReadinessReason() const { return WorldReadinessReason; }
	bool IsWorldReadyForPlay() const { return WorldReadiness == EAuraWorldReadiness::Ready; }
	int32 GetLoadedMonsterSpawnRowCount() const { return LoadedMonsterSpawnRows.Num(); }
	static EAuraWorldReadiness EvaluateWorldReadiness(bool bRoleReady, bool bDispatcherReady, bool bDirectorReady,
		bool bPopulationReady, bool bCrossValidationReady, bool bPopulationFinalized);

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

#if !UE_BUILD_SHIPPING
	/** Development-only automation fixture, registered by the non-Shipping console path. */
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;
#endif

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
	void HandleAuthoritativeDeath(const FAuraDeathEvent& Event);
	void FinalizeRoleBattleStartup();
	void SetWorldReadiness(EAuraWorldReadiness NewReadiness, const FString& Reason);
	void ScheduleDedicatedServerReadyNotification();
	void RunRoleBattleDays1012NetworkProbe();
	void RunRoleBattleDay16NetworkProbe();
	void RunRoleBattleDay17NetworkProbe();
	void RunRoleBattleDay18PersistenceProbe();
	void RunRoleBattleDay19NetworkProbe();

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
	FTimerHandle RoleBattleStartupTimerHandle;
	FTimerHandle RoleBattleDay16ProbeTimerHandle;
	FTimerHandle RoleBattleDay17ProbeTimerHandle;
	FTimerHandle Day17ProbeCloseTimerHandle;
	FTimerHandle RoleBattleDay18ProbeTimerHandle;
	FTimerHandle RoleBattleDay19ProbeTimerHandle;
	EAuraWorldReadiness WorldReadiness = EAuraWorldReadiness::Initializing;
	FString WorldReadinessReason = TEXT("Role/Battle startup is initializing.");
	/** True only when the economy registry was loaded successfully for this map instance. */
	bool bEconomyRegistryLoadedForCurrentWorld = false;
	bool bSkipInitialPopulationForLegacyDay8Probe = false;
	bool bDay16ProbeFixtureMutationApplied = false;
	bool bDay16ProbeRespawnRequested = false;
	TWeakObjectPtr<APawn> Day16ProbePreviousPawn;
	int64 Day16ProbeExpectedBalance = 0;
	int32 Day16ProbeExpectedInitializationCount = 0;
	bool bDay17ProbeFixtureReady = false;
	bool bDay18ProbeFixtureReady = false;
	bool bDay19ProbeFixtureReady = false;

	FTimerHandle RoleConfigPollTimerHandle;

};
