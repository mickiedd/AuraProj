// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Game/AuraPlayerProfileIdentity.h"
#include "Game/AuraSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AuraPersistenceSubsystem.generated.h"

class AAuraPlayerState;
class APlayerController;
class UAuraAbilitySystemComponent;
class UAuraAttributeSet;
class UAuraPlayerSaveGame;
class ULoadScreenSaveGame;
struct FUniqueNetIdRepl;

/** Authority-owned profile/world persistence coordinator. */
UCLASS()
class AURA_API UAuraPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Reads the server-owned world identity/provider policy before any remote login. */
	bool ConfigureAuthorityWorld(const FString& MapName, FString& OutError, FName MapId = NAME_None);
	bool IsPersistentLoginRequired() const { return bPersistentLoginRequired; }
	bool IsAuthorityPersistenceEnabled() const { return bAuthorityPersistenceEnabled; }
	const FString& GetWorldPersistenceId() const { return WorldPersistenceId; }
	const FString& GetExpectedProviderName() const { return ExpectedProviderName; }

	/** Resolves an authenticated provider ID; the fixture branch is development-only and allow-listed. */
	bool ResolveConnectionIdentity(const FUniqueNetIdRepl& NetId, const FString& Options,
		FAuraPlayerProfileId& OutIdentity, FString& OutError) const;

	/** Loads or creates a profile before HandleStartingNewPlayer can spawn a pawn. */
	bool PreparePlayerProfile(APlayerController* NewPlayerController, const FUniqueNetIdRepl& NetId,
		const FString& Options, FName RequestedRole, FString& OutError);
	bool ApplyPreparedProfile(AAuraPlayerState* PlayerState, FString& OutError) const;
	bool SavePlayerProfile(AAuraPlayerState* PlayerState, const UAuraAbilitySystemComponent* AbilitySystemComponent = nullptr,
		const UAuraAttributeSet* AttributeSet = nullptr, FString* OutError = nullptr);
	/** Commits a player and world checkpoint under one manifest generation. */
	bool SavePlayerAndWorldCheckpoint(AAuraPlayerState* PlayerState,
		const UAuraAbilitySystemComponent* AbilitySystemComponent, const UAuraAttributeSet* AttributeSet,
		const FAuraPopulationDebugSnapshot& PopulationSnapshot,
		const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError);
	void ReleasePlayerProfile(AAuraPlayerState* PlayerState);

	/** Loads the highest valid manifest/world record at most once per authoritative world generation. */
	bool LoadWorldState(const FString& MapName, FString& OutError, FName ExpectedMapId = NAME_None);
	bool SaveWorldState(const FAuraPopulationDebugSnapshot& PopulationSnapshot,
		const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError);
	bool IsWorldRestoreComplete() const { return bWorldRestoreAttempted && bWorldRestoreReady; }
	bool HasLoadedWorldSnapshot() const { return LoadedWorldSave.Get() != nullptr; }
	const TArray<FAuraPopulationSlotSnapshot>& GetLoadedPopulationSnapshot() const { return LoadedPopulationSnapshot; }
	const TArray<FAuraPersistedMerchantStock>& GetLoadedMerchantStocks() const { return LoadedMerchantStocks; }

	static bool ValidateWorldPersistenceId(const FString& InWorldPersistenceId, FString& OutError);
	static bool ValidatePlayerSaveRecord(const UAuraPlayerSaveGame& Save, const FAuraPlayerProfileId& ExpectedIdentity,
		const FString& ExpectedProviderName, FString& OutError);
	static bool ValidateWorldSaveRecord(const class UAuraWorldSaveGame& Save, const FString& ExpectedWorldPersistenceId,
		FString& OutError, FName ExpectedMapId = NAME_None);
	static FString ComputePlayerSaveChecksum(const UAuraPlayerSaveGame& Save);
	static FString ComputeWorldSaveChecksum(const class UAuraWorldSaveGame& Save);
	static FString ComputeManifestChecksum(const class UAuraPersistenceManifestSaveGame& Manifest);
	static bool MigrateLegacySave(const ULoadScreenSaveGame& Legacy, const FAuraPlayerProfileId& Identity,
		FName RequestedRole, UAuraPlayerSaveGame& OutSave, FString& OutError);

private:
	void ResetWorldRestoreState();
	bool LoadOrCreatePlayerSave(const FAuraPlayerProfileId& Identity, FName RequestedRole,
		UAuraPlayerSaveGame*& OutSave, FString& OutError);
	bool LoadManifestCandidate(const FString& SlotName, class UAuraPersistenceManifestSaveGame*& OutManifest) const;
	bool ValidateManifest(const class UAuraPersistenceManifestSaveGame& Manifest, FString& OutError) const;
	bool WriteManifest(int32 Generation, FString& OutError);
	bool SavePlayerProfileRecord(AAuraPlayerState* PlayerState, const UAuraAbilitySystemComponent* AbilitySystemComponent,
		const UAuraAttributeSet* AttributeSet, FString& OutError);
	bool SaveWorldStateRecord(const FAuraPopulationDebugSnapshot& PopulationSnapshot,
		const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError);
	FString BuildWorldRecordSlot(int32 Generation) const;
	FString BuildManifestSlot(bool bUseA) const;
	FString GetLegacySlotName(const FAuraPlayerProfileId& Identity) const;
	void CaptureAbilityState(UAuraPlayerSaveGame& Save, const UAuraAbilitySystemComponent* AbilitySystemComponent) const;

	FString WorldPersistenceId;
	FString ExpectedProviderName;
	FString CurrentWorldMapName;
	FName CurrentWorldMapId = NAME_None;
	FString ActiveManifestSlot;
	int32 ActiveManifestGeneration = 0;
	int32 WorldGeneration = 0;
	bool bAuthorityPersistenceEnabled = false;
	bool bPersistentLoginRequired = false;
	bool bWorldRestoreAttempted = false;
	bool bWorldRestoreReady = false;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAuraPlayerSaveGame>> PreparedProfiles;

	TMap<FString, int32> PlayerGenerations;
	TMap<FString, FAuraPersistenceManifestRecord> PlayerManifestRecords;
	TMap<TWeakObjectPtr<AAuraPlayerState>, FString> ActiveProfileKeys;
	TSet<FString> ActiveProfiles;

	UPROPERTY(Transient)
	TObjectPtr<class UAuraWorldSaveGame> LoadedWorldSave;

	TArray<FAuraPopulationSlotSnapshot> LoadedPopulationSnapshot;
	TArray<FAuraPersistedMerchantStock> LoadedMerchantStocks;
	FAuraPersistenceManifestRecord WorldManifestRecord;
};
