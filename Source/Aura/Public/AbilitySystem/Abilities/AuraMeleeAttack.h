// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AuraMeleeAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputPress;
class UAnimMontage;

/**
 * Crunch-style authored melee combo. The montage owns the timing: gameplay
 * notifies open/close the input window and report each damage frame. Damage is
 * still routed through Aura's authoritative combat boundary.
 */
UCLASS()
class AURA_API UAuraMeleeAttack : public UAuraDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraMeleeAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

#if !UE_BUILD_SHIPPING
	/** Development observability for local and cross-process migration probes. */
	int32 GetTestOpenEventCount() const { return TestOpenEventCount; }
	int32 GetTestDamageEventCount() const { return TestDamageEventCount; }
	int32 GetTestCloseEventCount() const { return TestCloseEventCount; }
	int32 GetTestAcceptedDamageCount() const { return TestAcceptedDamageCount; }
	int32 GetTestAcceptedDamageSectionMask() const { return TestAcceptedDamageSectionMask; }
	int32 GetTestComboIndex() const { return CurrentComboIndex; }
	bool IsTestComboWindowOpen() const { return bComboWindowOpen; }
	bool IsTestAuthorityFallbackTimelineActive() const { return bAuthorityFallbackTimelineActive; }
	bool HasTestQueuedSuccessor() const { return bAuthorityAdvanceQueued; }
	bool HasTestInputPressTask() const { return InputPressTask != nullptr; }
	bool HasTestAuthorityFallbackTimerPending() const;
#endif

protected:
	/** Four authored sections, normally named Combo01 through Combo04. */
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Animation")
	TObjectPtr<UAnimMontage> ComboMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Animation")
	TArray<FName> ComboSections = { FName("Combo01"), FName("Combo02"), FName("Combo03"), FName("Combo04") };

	/** Montage event emitted on each authored impact frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Events")
	FGameplayTag DamageEventTag;

	/** Montage events that delimit the window in which the next click is accepted. */
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Events")
	FGameplayTag ComboWindowOpenTag;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Events")
	FGameplayTag ComboWindowCloseTag;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Damage")
	float DamageRadius = 150.f;

	/** Fallback used until the imported role asset supplies its damage tuning. */
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Combo|Damage")
	float DefaultComboDamage = 25.f;

private:
	UFUNCTION()
	void OnMontageEvent(FGameplayEventData EventData);

	UFUNCTION()
	void OnInputWindowOpened(FGameplayEventData EventData);

	UFUNCTION()
	void OnInputWindowClosed(FGameplayEventData EventData);

	UFUNCTION()
	void OnInputPressed(float TimeWaited);

	void ArmInputPressTask();
	void SyncComboIndexToMontageSection();
	void ArmAuthorityFallbackForSection(int32 SectionIndex);
	void HandleAuthorityFallbackOpen(int32 SectionIndex);
	void HandleAuthorityFallbackDamage(int32 SectionIndex);
	void HandleAuthorityFallbackClose(int32 SectionIndex);
	void ApplyComboDamage();
	void CleanupTasks();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowOpenTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WindowCloseTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> InputPressTask;

	int32 CurrentComboIndex = 0;
	bool bComboWindowOpen = false;
	bool bDamageConsumedForCurrentComboSection = false;
	bool bAuthorityAdvanceQueued = false;
	bool bAuthorityFallbackTimelineActive = false;
	bool bAuthorityFallbackEventContext = false;
	int32 AuthorityFallbackEventSection = INDEX_NONE;
	uint32 OpenEventSectionMask = 0;
	uint32 DamageEventSectionMask = 0;
	uint32 CloseEventSectionMask = 0;
	FTimerHandle AuthorityFallbackOpenTimer;
	FTimerHandle AuthorityFallbackDamageTimer;
	FTimerHandle AuthorityFallbackCloseTimer;

#if !UE_BUILD_SHIPPING
	int32 TestOpenEventCount = 0;
	int32 TestDamageEventCount = 0;
	int32 TestCloseEventCount = 0;
	int32 TestAcceptedDamageCount = 0;
	int32 TestAcceptedDamageSectionMask = 0;
#endif
};
