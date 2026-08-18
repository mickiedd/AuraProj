// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AuraAbilitySystemComponent.generated.h"

class ULoadScreenSaveGame;
struct FAuraAbilityInfo;
class UAuraDataAbility;
class UAuraAbilityDefinition;
struct FRoleDefaultInfo;

UENUM(BlueprintType)
enum class EAuraAbilityGrantSource : uint8
{
	Unknown,
	Role,
	Progression
};

/** Persistent server ledger; it lives with the PlayerState-owned ASC across pawns. */
USTRUCT(BlueprintType)
struct AURA_API FAuraRoleGrantLedger
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	FName GrantedRoleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	int32 RoleDefinitionVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	TArray<FActiveGameplayEffectHandle> RemovableEffectHandles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	bool bReconciled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Role Grants")
	bool bAttributesInitialized = false;
};

struct AURA_API FAuraRoleGrantTransactionSnapshot
{
	FAuraRoleGrantLedger Ledger;
	bool bStartupAbilitiesGiven = false;
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;
	TArray<TObjectPtr<UAuraAbilityDefinition>> GrantedAbilityDefinitions;
	TMap<FGameplayAbilitySpecHandle, FName> GrantedRoleBySpecHandle;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/);
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAbilityStatusChanged, const FGameplayTag& /*AbilityTag*/, const FGameplayTag& /*StatusTag*/, int32 /*AbilityLevel*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FAbilityEquipped, const FGameplayTag& /*AbilityTag*/, const FGameplayTag& /*Status*/, const FGameplayTag& /*Slot*/, const FGameplayTag& /*PrevSlot*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDeactivatePassiveAbility, const FGameplayTag& /*AbilityTag*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FActivatePassiveEffect, const FGameplayTag& /*AbilityTag*/, bool /*bActivate*/);

/**
 * 
 */
UCLASS()
class AURA_API UAuraAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	void AbilityActorInfoSet();

	FEffectAssetTags EffectAssetTags;
	FAbilitiesGiven AbilitiesGivenDelegate;
	FAbilityStatusChanged AbilityStatusChanged;
	FAbilityEquipped AbilityEquipped;
	FDeactivatePassiveAbility DeactivatePassiveAbility;
	FActivatePassiveEffect ActivatePassiveEffect;

 	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);
 	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities);
	void AddCharacterDataAbilities(const TArray<UAuraAbilityDefinition*>& Definitions, int32 AbilityLevel = 1);
 	void AddCharacterDataPassiveAbilities(const TArray<UAuraAbilityDefinition*>& Definitions);
	/** Compatibility mirror for existing UI; the role ledger is the authoritative guard. */
	bool bStartupAbilitiesGiven = false;

	/** Validates all stable tags and save provenance without mutating specs or the ledger. */
	bool ValidateRoleGrantSet(FName RoleId, const FRoleDefaultInfo& Role, const ULoadScreenSaveGame* SaveData, FString& OutError) const;

	/** Restores/reconciles once, or verifies a same-role respawn without duplicate grants. */
	bool ApplyRoleGrantSet(FName RoleId, int32 RoleDefinitionVersion, const FRoleDefaultInfo& Role,
		const ULoadScreenSaveGame* SaveData, FString& OutError);

	const FAuraRoleGrantLedger& GetRoleGrantLedger() const { return RoleGrantLedger; }
	void MarkRoleAttributesInitialized() { RoleGrantLedger.bAttributesInitialized = true; }
	void CaptureRoleGrantState(FAuraRoleGrantTransactionSnapshot& OutSnapshot) const;
	void RollbackRoleGrantState(const FAuraRoleGrantTransactionSnapshot& Snapshot);
	EAuraAbilityGrantSource GetGrantSourceForSpec(const FGameplayAbilitySpec& AbilitySpec, FName& OutGrantedRoleId) const;
	int32 CountAbilitySpecsByTag(const FGameplayTag& AbilityTag) const;

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagHeld(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ForEachAbility(const FForEachAbility& Delegate);

	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	FAuraAbilityInfo GetRuntimeAbilityInfoForSpec(const FGameplayAbilitySpec& AbilitySpec) const;
	FAuraAbilityInfo GetRuntimeAbilityInfoForTag(const FGameplayTag& AbilityTag) const;
	FGameplayTag GetStatusFromAbilityTag(const FGameplayTag& AbilityTag);
	FGameplayTag GetSlotFromAbilityTag(const FGameplayTag& AbilityTag);
	bool SlotIsEmpty(const FGameplayTag& Slot);
	static bool AbilityHasSlot(const FGameplayAbilitySpec& Spec, const FGameplayTag& Slot);
	static bool AbilityHasAnySlot(const FGameplayAbilitySpec& Spec);
	FGameplayAbilitySpec* GetSpecWithSlot(const FGameplayTag& Slot);
	bool IsPassiveAbility(const FGameplayAbilitySpec& Spec) const;
	static void AssignSlotToAbility(FGameplayAbilitySpec& Spec, const FGameplayTag& Slot);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastActivatePassiveEffect(const FGameplayTag& AbilityTag, bool bActivate);

	FGameplayAbilitySpec* GetSpecFromAbilityTag(const FGameplayTag& AbilityTag);

	void UpgradeAttribute(const FGameplayTag& AttributeTag);

	UFUNCTION(Server, Reliable)
	void ServerUpgradeAttribute(const FGameplayTag& AttributeTag);

	void UpdateAbilityStatuses(int32 Level);
	void GrantAndEquipAllAbilities();

	UFUNCTION(Server, Reliable)
	void ServerSpendSpellPoint(const FGameplayTag& AbilityTag);

	UFUNCTION(Server, Reliable)
	void ServerEquipAbility(const FGameplayTag& AbilityTag, const FGameplayTag& Slot);

	/** Server-authoritative activation request. Clients must call this instead of TryActivateAbility directly. */
	UFUNCTION(Server, Reliable)
	void ServerRequestActivateAbility(FGameplayAbilitySpecHandle AbilityHandle);

	UFUNCTION(Client, Reliable)
	void ClientEquipAbility(const FGameplayTag& AbilityTag, const FGameplayTag& Status, const FGameplayTag& Slot, const FGameplayTag& PreviousSlot);

	bool GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, FString& OutDescription, FString& OutNextLevelDescription);

	static void ClearSlot(FGameplayAbilitySpec* Spec);
	void ClearAbilitiesOfSlot(const FGameplayTag& Slot);
	static bool AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot);
protected:
	/** Keeps transient XML definitions alive; FGameplayAbilitySpec::SourceObject is weak. */
	UPROPERTY()
	TArray<TObjectPtr<UAuraAbilityDefinition>> GrantedAbilityDefinitions;

	UPROPERTY(VisibleInstanceOnly, Category = "Role Grants")
	FAuraRoleGrantLedger RoleGrantLedger;

	virtual void OnRep_ActivateAbilities() override;

	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);

	UFUNCTION(Client, Reliable)
	void ClientUpdateAbilityStatus(const FGameplayTag& AbilityTag, const FGameplayTag& StatusTag, int32 AbilityLevel);

private:
	void SetAbilityStatus(FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& StatusTag);
	bool EquipAbilityToSlot(FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& Slot, FGameplayTag& OutPreviousSlot);
	FGameplayTag FindSlotForAbility(const FAuraAbilityInfo& AbilityInfo, const FGameplayAbilitySpec& AbilitySpec, const TSet<FGameplayTag>& ReservedSlots) const;

	// Prevents TryActivateAbility spam while an input is held and activation is blocked by cost/cooldown.
	TMap<FGameplayTag, float> NextAllowedInputTagTryTime;
	float HeldRetryDelay = 0.10f;
	float HeldCooldownRetryDelay = 0.12f;
	float HeldCostRetryDelay = 0.20f;
	float HeldSuccessRetryDelay = 0.03f;

	/** Role IDs remain stable data IDs rather than being converted to gameplay tags. */
	TMap<FGameplayAbilitySpecHandle, FName> GrantedRoleBySpecHandle;
};
