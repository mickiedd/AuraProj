// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AuraCharacterAnimInstance.generated.h"

class AAuraCharacterBase;
class AAuraBroomVehicle;
class UCharacterMovementComponent;

/**
 * Animation instance for the Aura player character.
 *
 * All properties are updated each frame in NativeUpdateAnimation and are
 * exposed as BlueprintReadOnly so that the Animation Blueprint state machine
 * can reference them directly as transition conditions or blend-tree inputs.
 *
 * State machine overview (matches the ABP state machine):
 *   Entry → Idle
 *   Idle  ↔ Running         (GroundSpeed / bShouldMove)
 *   Idle  ↔ Cast_Shock_Loop (bIsBeingShocked)
 *   Idle  → Sit             (bIsMounted – character is riding the broom)
 *   Sit   → Idle            (!bIsMounted)
 *   Idle  → ToStun → Stun   (bIsStunned)
 *   Stun  → Idle            (!bIsStunned)
 *   Idle  → ToDead → Dead   (bIsDead)
 *   Idle  → ToInAir → InAir (bIsInAir)
 *   InAir → Idle            (!bIsInAir)
 */
UCLASS()
class AURA_API UAuraCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ── Movement ─────────────────────────────────────────────────────────────

	/** Horizontal ground speed (XY plane). Use this as a blend-space driver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float GroundSpeed = 0.f;

	/**
	 * True when the character should be in a locomotion pose.
	 * Equivalent to GroundSpeed > 0 AND not mounted AND not dead AND not stunned.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bShouldMove = false;

	/** True while the character is airborne (in-air / jump). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsInAir = false;

	// ── Broom ─────────────────────────────────────────────────────────────────

	/**
	 * True while the character is attached to the broom vehicle.
	 * Drives the Idle→Sit and Sit→Idle transitions.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom")
	bool bIsMounted = false;

	// ── Combat / Status ───────────────────────────────────────────────────────

	/** True while the Stun debuff is active. Drives Idle↔ToStun/Stun. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsStunned = false;

	/** True while the character is dead. Drives Idle→ToDead→Dead. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsDead = false;

	/**
	 * True while the character is being shocked (Cast_Shock_Loop state).
	 * Mirrors AAuraCharacterBase::bIsBeingShocked.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsBeingShocked = false;

private:
	UPROPERTY()
	TObjectPtr<AAuraCharacterBase> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> OwningMovement;
};
