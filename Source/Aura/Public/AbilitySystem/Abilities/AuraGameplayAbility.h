// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AuraGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class AURA_API UAuraGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:

	UPROPERTY(EditDefaultsOnly, Category="Input")
	FGameplayTag StartupInputTag;

	/** Returns the input slot used when granting this ability. Derived abilities may resolve it lazily. */
	virtual FGameplayTag GetStartupInputTag() const { return StartupInputTag; }

	virtual FString GetDescription(int32 Level);
	virtual FString GetNextLevelDescription(int32 Level);
	static FString GetLockedDescription(int32 Level);

	/**
	 * Skip the cost check on non-authoritative clients — attributes (e.g. Mana) may not
	 * have replicated yet. The server always performs the authoritative check on activation.
	 */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:

	float GetManaCost(float InLevel = 1.f) const;
	float GetCooldown(float InLevel = 1.f) const;
};
