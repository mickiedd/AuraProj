// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/AuraAttackTimelineTypes.h"
#include "AuraAttackTimelineComponent.generated.h"

/**
 * Thin authority-owned host for the Day 44 timeline contract. It intentionally
 * does not apply damage, bind input, invoke movement, or create presentation cues.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraAttackTimelineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraAttackTimelineComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EAuraAttackTimelineResult TryBeginWindup(const FAuraAttackTimelineKey& InKey,
		const FAuraAttackTimelineDefinition& Definition, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryCommit(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryResolveImpact(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryFinishRecovery(const FAuraAttackTimelineKey& CallbackKey, double AuthorityNow, FString& OutError);
	EAuraAttackTimelineResult TryCancel(const FAuraAttackTimelineKey& CallbackKey, FName Reason, FString& OutError);

	const FAuraAttackTimelineState& GetState() const { return State; }
	/** Clears a terminal Cancelled state before the next attack is admitted. */
	void ResetTimeline();

private:
	bool HasAuthorityContext(FString& OutError) const;

	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing = OnRep_State, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gameplay|Attack", meta = (AllowPrivateAccess = "true"))
	FAuraAttackTimelineState State;
};
