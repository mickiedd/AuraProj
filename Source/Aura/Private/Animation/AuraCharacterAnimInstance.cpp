// Copyright Druid Mechanics

#include "Animation/AuraCharacterAnimInstance.h"

#include "Character/AuraCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/CombatInterface.h"

void UAuraCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<AAuraCharacterBase>(TryGetPawnOwner());
	if (IsValid(OwningCharacter))
	{
		OwningMovement = OwningCharacter->GetCharacterMovement();
	}
}

void UAuraCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(OwningCharacter) || !IsValid(OwningMovement))
	{
		return;
	}

	// ── Movement ─────────────────────────────────────────────────────────────

	GroundSpeed = OwningCharacter->GetVelocity().Size2D();
	bIsInAir    = OwningMovement->IsFalling();

	// ── Broom mount ──────────────────────────────────────────────────────────

	// Read the authoritative replicated flag set by AuraBroomVehicle::ApplyMountedState.
	bIsMounted = OwningCharacter->bIsMounted;

	// ── Status flags ─────────────────────────────────────────────────────────

	bIsStunned      = OwningCharacter->bIsStunned;
	bIsDead         = ICombatInterface::Execute_IsDead(OwningCharacter);
	bIsBeingShocked = OwningCharacter->bIsBeingShocked;

	// ── Composite helper ─────────────────────────────────────────────────────

	// Character should show locomotion only when freely moving on the ground.
	bShouldMove = GroundSpeed > 0.f
	           && !bIsInAir
	           && !bIsMounted
	           && !bIsDead
	           && !bIsStunned;
}
