// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Character/AuraCharacterBase.h"
#include "Interaction/PlayerInterface.h"
#include "AuraCharacter.generated.h"

class UNiagaraComponent;
class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class AAuraPlayerState;
class UAuraBuildingComponent;
struct FHitResult;
/**
 * 
 */
UCLASS()
class AURA_API AAuraCharacter : public AAuraCharacterBase, public IPlayerInterface
{
	GENERATED_BODY()
public:
	AAuraCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** Player Interface */
	virtual void AddToXP_Implementation(int32 InXP) override;
	virtual void LevelUp_Implementation() override;
	virtual int32 GetXP_Implementation() const override;
	virtual int32 FindLevelForXP_Implementation(int32 InXP) const override;
	virtual int32 GetAttributePointsReward_Implementation(int32 Level) const override;
	virtual int32 GetSpellPointsReward_Implementation(int32 Level) const override;
	virtual void AddToPlayerLevel_Implementation(int32 InPlayerLevel) override;
	virtual void AddToAttributePoints_Implementation(int32 InAttributePoints) override;
	virtual void AddToSpellPoints_Implementation(int32 InSpellPoints) override;
	virtual int32 GetAttributePoints_Implementation() const override;
	virtual int32 GetSpellPoints_Implementation() const override;
	virtual void ShowMagicCircle_Implementation(UMaterialInterface* DecalMaterial) override;
	virtual void HideMagicCircle_Implementation() override;
	virtual void SaveProgress_Implementation(const FName& CheckpointTag) override;
	/** end Player Interface */

	/** Combat Interface */
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;
	/** end Combat Interface */

	/** Mount Status Interface */
	virtual bool IsMounted_Implementation() const override { return bIsMounted; }
	/** end Mount Status Interface */

	UPROPERTY(EditDefaultsOnly)
	float DeathTime = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Fall")
	bool bEnableFallDeath = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Fall", meta = (ClampMin = "0.1"))
	float FatalFallDelay = 2.5f;

	FTimerHandle DeathTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraComponent> LevelUpNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UAuraBuildingComponent> BuildingComponent;

	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

	void LoadProgress();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateFatalFallState(float DeltaSeconds);
	void ResetFatalFallState();

	void BindPlayerNameDelegate();
	void UpdateOverheadPlayerName();
	void UpdateOverheadNameFacingCamera();
	void HandlePlayerNameChanged(const FString& NewName);

	float CurrentFallDuration = 0.f;
	bool bWasInFatalFallWindow = false;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UTextRenderComponent> OverheadNameText;

	UPROPERTY(Transient)
	TObjectPtr<AAuraPlayerState> BoundPlayerState;
	
	virtual void InitAbilityActorInfo() override;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLevelUpParticles() const;
};
