// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.h"
#include "AuraCrunchUppercut.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputPress;
class UAnimMontage;

/** Crunch UpperCut translated from the source launch + airborne follow-up chain. */
UCLASS()
class AURA_API UAuraCrunchUppercut : public UAuraCrunchAbilityBase
{
	GENERATED_BODY()

public:
	UAuraCrunchUppercut();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Animation")
	TObjectPtr<UAnimMontage> UpperCutMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Movement")
	float UpperCutLaunchSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Movement")
	float UpperCutHoldSpeed = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Targeting")
	float UpperCutTargetRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Targeting")
	float UpperCutTargetRange = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Timing")
	float LaunchEventTime = 0.296f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Timing")
	TArray<float> DamageEventTimes = { 0.542f, 2.272f, 2.680f };

	UPROPERTY(EditDefaultsOnly, Category = "Crunch UpperCut|Timing")
	TArray<FName> FollowupSections = { FName("combo02"), FName("combo03") };

private:
	UFUNCTION()
	void HandleLaunchEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleDamageEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleFollowupInput(float TimeWaited);

	void CommitLaunch();
	void CommitDamageEvent(int32 EventIndex, const FGameplayAbilityTargetDataHandle* OptionalTargetData = nullptr);
	void ArmFollowupInput();
	void ScheduleSourceTimingFallbacks();
	void OpenFollowupWindow();
	void CloseFollowupWindow();
	void HandleUpperCutTimeout();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> LaunchEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> FollowupInputTask;

	FTimerHandle UpperCutTimeoutTimer;

	int32 NextFollowupIndex = 0;
	int32 NextDamageEventIndex = 0;
	bool bLaunchCommitted = false;
	bool bFollowupWindowOpen = false;
};
