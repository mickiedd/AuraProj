// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "AuraRoleApplicationTestActor.generated.h"

class UAuraAbilitySystemComponent;

/** UHT-visible non-shipping shell used to exercise actor-owned and persistent ASC role application. */
UCLASS(Transient, NotBlueprintable)
class AAuraRoleApplicationTestActor : public AAuraCharacterBase
{
	GENERATED_BODY()

public:
	AAuraRoleApplicationTestActor();

	void ConfigureRoleShell(const FGameplayTag& EntityType, const FGameplayTag& ControlType);
	void InitializeCombatIdentityForTest(const FAuraCombatIdentity& Identity);
	void UseExternalAbilitySystem(UAuraAbilitySystemComponent* ExternalASC, UAttributeSet* ExternalAttributes);
	bool InitializeTestAbilityActorInfo(AActor* OwnerActor = nullptr);

	UAuraAbilitySystemComponent* GetTestASC() const;
	USkeletalMesh* GetEquippedWeaponMesh() const;
	FName GetWeaponAttachSocketForTest() const;
	bool IsWeaponAttachedToBodyForTest() const;
	FName GetWeaponTipSocketForTest() const { return WeaponTipSocketName; }

protected:
	virtual FAuraCombatIdentity BuildDefaultCombatIdentity() const override;
	virtual FGameplayTag GetRequiredRoleEntityType() const override;
	virtual FGameplayTag GetRequiredRoleControlType() const override;

private:
	FGameplayTag RequiredEntityType;
	FGameplayTag RequiredControlType;
};
