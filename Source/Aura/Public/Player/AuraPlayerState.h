// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Economy/AuraEconomyTypes.h"
#include "Game/AuraPlayerProfileIdentity.h"
#include "GameFramework/PlayerState.h"
#include "AuraAbilityGraph/Public/AbilityGraphTypes.h"
#include "AuraPlayerState.generated.h"


class UAbilitySystemComponent;
class UAttributeSet;
class ULevelUpInfo;
class UAuraCurrencyComponent;
class UAuraInventoryComponent;
class UAuraPlayerSaveGame;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStatChanged, int32 /*StatValue*/)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLevelChanged, int32 /*StatValue*/, bool /*bLevelUp*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerNameChanged, const FString& /*PlayerName*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoleChanged, FName /*Role*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFirearmStateChanged, const struct FAuraFirearmState& /*State*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTutorialProgressChanged, uint32 /*CompletionMask*/)

USTRUCT(BlueprintType)
struct AURA_API FAuraFirearmState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	bool bApplicable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	int32 MagazineCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	int32 MagazineRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	int32 ReserveCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	int32 ReserveRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	float ReloadDuration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	bool bReloading = false;

	UPROPERTY()
	uint32 ReloadSerial = 0;

	UPROPERTY()
	uint32 AmmoRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	FName FireMode = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Firearm")
	float MinimumShotInterval = 0.f;
};

/**
 * 
 */
UCLASS()
class AURA_API AAuraPlayerState : public APlayerState, public IAbilitySystemInterface, public IAuraFirearmAuthority,
	public IAuraAbilityCommitAuthority
{
	GENERATED_BODY()
public:
	AAuraPlayerState();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void SetPlayerName(const FString& S) override;
	virtual void OnRep_PlayerName() override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }
	UAuraCurrencyComponent* GetCurrencyComponent() const { return CurrencyComponent; }
	UAuraInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** Initializes configured starting currency once for this authority-owned PlayerState. */
	bool InitializeEconomyForNewProfileOnce();
	/** Applies a validated persistent profile before the pawn's role transaction. */
	bool ApplyPersistentProfile(const UAuraPlayerSaveGame& SaveData, FString& OutError);
	EAuraEconomyInitializationState GetEconomyInitializationState() const { return EconomyInitializationState; }
	int32 GetEconomyInitializationCount() const { return EconomyInitializationCount; }
	/** Logout may checkpoint only after economy, role, and default attributes are committed. */
	bool IsReadyForPersistentSave() const;
	void SetProfileIdentity(const FAuraPlayerProfileId& InIdentity) { ProfileIdentity = InIdentity; }
	const FAuraPlayerProfileId& GetProfileIdentity() const { return ProfileIdentity; }
	bool HasPersistentProfileIdentity() const { return ProfileIdentity.IsValid(); }
	void SetPendingPersistentProfile(UAuraPlayerSaveGame* InProfile) { PendingPersistentProfile = InProfile; }
	UAuraPlayerSaveGame* GetPendingPersistentProfile() const { return PendingPersistentProfile; }

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<ULevelUpInfo> LevelUpInfo;

	FOnPlayerStatChanged OnXPChangedDelegate;
	FOnLevelChanged OnLevelChangedDelegate;
	FOnPlayerStatChanged OnAttributePointsChangedDelegate;
	FOnPlayerStatChanged OnSpellPointsChangedDelegate;
	FOnPlayerNameChanged OnPlayerNameChangedDelegate;
	FOnRoleChanged OnRoleChangedDelegate;

	FORCEINLINE int32 GetPlayerLevel() const { return Level; }
	FORCEINLINE int32 GetXP() const { return XP; }
	FORCEINLINE int32 GetAttributePoints() const { return AttributePoints; }
	FORCEINLINE int32 GetSpellPoints() const { return SpellPoints; }
	FORCEINLINE FName GetRole() const { return CharacterRole; }
	FORCEINLINE FName GetPendingAcceptedRoleId() const { return PendingAcceptedRoleId; }
	FORCEINLINE bool HasPendingAcceptedRoleId() const { return !PendingAcceptedRoleId.IsNone(); }
	void SetPendingAcceptedRoleId(FName InRoleId) { PendingAcceptedRoleId = InRoleId; }
	void ClearPendingAcceptedRoleId() { PendingAcceptedRoleId = NAME_None; }

	void AddToXP(int32 InXP);
	void AddToLevel(int32 InLevel);
	void AddToAttributePoints(int32 InPoints);
	void AddToSpellPoints(int32 InPoints);

	void SetXP(int32 InXP);
	void SetLevel(int32 InLevel);
	void SetAttributePoints(int32 InPoints);
	void SetSpellPoints(int32 InPoints);
	/** Authority-only persistent role commit. Returns false for client mutation attempts. */
	bool SetRole(FName InRole);

	/** Stable player-owned firearm ledger. Aura returns an explicit non-applicable state. */
	const FAuraFirearmState& GetFirearmState() const { return FirearmState; }
	bool InitializeFirearmForRole(FName InRole, int32 InMagazineCapacity = 12, int32 InReserveCapacity = 48,
		float InReloadDuration = 1.25f, float InMinimumShotInterval = 0.2f);
	bool BeginFirearmReload(FName& OutResultCode);
	void CancelFirearmReload(const TCHAR* Reason);
	bool TryConsumeFirearmRound(FName AbilityId, AActor* AvatarActor, FName& OutResultCode) override;
	void NotifyFirearmShotAccepted(FName AbilityId) override;
	void NotifyAuthoritativeAbilityCommitted(FName AbilityId) override;
	FOnFirearmStateChanged OnFirearmStateChanged;
	bool RestoreCompletedFirearmState(bool bInApplicable, int32 InMagazineCapacity, int32 InMagazineRounds,
		int32 InReserveCapacity, int32 InReserveRounds, float InReloadDuration, uint32 InAmmoRevision, FString& OutError);
	uint32 GetTutorialCompletionMask() const { return TutorialCompletionMask; }
	FName GetRecoveryState() const { return RecoveryState; }
	bool SetTutorialStepCompleted(uint8 StepIndex, bool bCompleted);
	bool ResetTutorialProgress();
	FOnTutorialProgressChanged OnTutorialProgressChanged;
	bool SetRecoveryState(FName InState);

	/** True after the persistent ASC has received its initial attribute set. */
	bool HasInitializedDefaultAttributes() const;
	void MarkDefaultAttributesInitialized();
	
protected:
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAuraCurrencyComponent> CurrencyComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAuraInventoryComponent> InventoryComponent;

private:

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_Level)
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_XP)
	int32 XP = 0;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_AttributePoints)
	int32 AttributePoints = 0;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_SpellPoints)
	int32 SpellPoints = 0;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_Role)
	FName CharacterRole = NAME_None;

	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_EconomyInitializationState, Transient)
	EAuraEconomyInitializationState EconomyInitializationState = EAuraEconomyInitializationState::NewEphemeralSession;

	UPROPERTY(VisibleInstanceOnly, Replicated, Transient)
	int32 EconomyInitializationCount = 0;

	/** Never replicated; the server uses it to bind a PlayerState to one profile record. */
	UPROPERTY(Transient)
	FAuraPlayerProfileId ProfileIdentity;

	UPROPERTY(Transient)
	TObjectPtr<UAuraPlayerSaveGame> PendingPersistentProfile;

	/** Authority-only connection request accepted by GameMode; Day 06 consumes this into CharacterRole. */
	UPROPERTY(Transient)
	FName PendingAcceptedRoleId = NAME_None;

	/** Owner-only replicated; completed values are the only values persisted. */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_FirearmState, Category = "Firearm")
	FAuraFirearmState FirearmState;

	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing = OnRep_TutorialCompletionMask, Category = "Tutorial")
	uint32 TutorialCompletionMask = 0;

	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Recovery")
	FName RecoveryState = TEXT("Alive");

	FTimerHandle FirearmReloadTimerHandle;
	double LastAcceptedFirearmShotTime = -1.0;

	void FinishFirearmReload(uint32 ExpectedSerial);

	UFUNCTION()
	void OnRep_Level(int32 OldLevel);

	UFUNCTION()
	void OnRep_XP(int32 OldXP);

	UFUNCTION()
	void OnRep_AttributePoints(int32 OldAttributePoints);

	UFUNCTION()
	void OnRep_SpellPoints(int32 OldSpellPoints);

	UFUNCTION()
	void OnRep_Role();

	UFUNCTION()
	void OnRep_EconomyInitializationState();

	UFUNCTION()
	void OnRep_FirearmState();

	UFUNCTION()
	void OnRep_TutorialCompletionMask();
};
