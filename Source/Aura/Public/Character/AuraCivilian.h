// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "AI/AuraCivilianBehaviorTypes.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "World/AuraPopulationTypes.h"
#include "AuraCivilian.generated.h"

class UWidgetComponent;
class AAuraCivilianAIController;

/**
 * Day 08 role-derived ambient actor. It owns one replicated ASC and one
 * AttributeSet, but has no player controller, loot path or offensive grants.
 */
UCLASS()
class AURA_API AAuraCivilian : public AAuraCharacterBase
{
	GENERATED_BODY()

public:
	AAuraCivilian();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetRequestedCivilianRoleId(FName InRoleId);
	FName GetRequestedCivilianRoleId() const { return RequestedCivilianRoleId; }

	/** Manager-only authority write; clients only observe the replicated state. */
	void SetPopulationMemberState(const FAuraPopulationMemberState& InState);
	const FAuraPopulationMemberState& GetPopulationMemberState() const { return PopulationMemberState; }

	/** Authority-only spawn correction; places the capsule on the traced floor. */
	bool AlignToGroundForSpawn();

	void SetCivilianActivity(EAuraCivilianActivity InActivity);
	EAuraCivilianActivity GetCivilianActivity() const { return CivilianActivity; }

protected:
	virtual void BeginPlay() override;
	virtual FAuraCombatIdentity BuildDefaultCombatIdentity() const override;
	virtual FGameplayTag GetRequiredRoleEntityType() const override;
	virtual FGameplayTag GetRequiredRoleControlType() const override;
	virtual void InitAbilityActorInfo() override;

	UFUNCTION()
	void OnRep_PopulationMemberState();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Civilian|Role")
	FName RequestedCivilianRoleId = FName(TEXT("Civilian"));

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_PopulationMemberState, Category = "Civilian|Population")
	FAuraPopulationMemberState PopulationMemberState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Civilian|AI")
	EAuraCivilianActivity CivilianActivity = EAuraCivilianActivity::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Civilian|UI")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	bool bHealthDelegatesBound = false;
};
