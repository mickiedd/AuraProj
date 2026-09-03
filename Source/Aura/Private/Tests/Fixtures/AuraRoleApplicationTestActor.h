// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "GameplayCueInterface.h"
#include "AuraRoleApplicationTestActor.generated.h"

class UAuraAbilitySystemComponent;

/** UHT-visible non-shipping shell used to exercise actor-owned and persistent ASC role application. */
UCLASS(Transient, NotBlueprintable)
class AAuraRoleApplicationTestActor : public AAuraCharacterBase, public IGameplayCueInterface
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

	/** Runtime cue probe used by migration automation; no gameplay state is changed. */
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag,
		EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;
	void ResetGameplayCueProbe();
	int32 GetGameplayCueProbeCount() const { return GameplayCueProbeCount; }
	FGameplayTag GetLastGameplayCueTag() const { return LastGameplayCueTag; }
	const FVector& GetLastGameplayCueLocation() const { return LastGameplayCueLocation; }
	AActor* GetLastGameplayCueEffectCauser() const { return LastGameplayCueEffectCauser.Get(); }
	AActor* GetLastGameplayCueInstigator() const { return LastGameplayCueInstigator.Get(); }
	const UObject* GetLastGameplayCueSourceObject() const { return LastGameplayCueSourceObject.Get(); }

protected:
	virtual FAuraCombatIdentity BuildDefaultCombatIdentity() const override;
	virtual FGameplayTag GetRequiredRoleEntityType() const override;
	virtual FGameplayTag GetRequiredRoleControlType() const override;

private:
	FGameplayTag RequiredEntityType;
	FGameplayTag RequiredControlType;
	int32 GameplayCueProbeCount = 0;
	FGameplayTag LastGameplayCueTag;
	FVector LastGameplayCueLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastGameplayCueEffectCauser;
	TWeakObjectPtr<AActor> LastGameplayCueInstigator;
	TWeakObjectPtr<const UObject> LastGameplayCueSourceObject;
};
