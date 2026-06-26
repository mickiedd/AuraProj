// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraBroomMotionComponent.generated.h"

class AAuraBroomVehicle;

/**
 * UAuraBroomMotionComponent
 *
 * Owns the broom's non-physics visual motion: idle hover (bob/sway/tilt while no
 * rider is present and no flight input is active) and smooth yaw interpolation
 * toward a flight target yaw. Extracted from AAuraBroomVehicle so the rideable-
 * pawn concern stays separate from the ambient-motion concern.
 *
 * Driven by the owning broom's Tick via TickMotion(), NOT by its own component
 * tick, so it runs in the correct order relative to the movement component
 * (flight integration first, then hover pin / yaw). Server-authoritative: hover
 * and yaw writes only happen on the server and replicate via the broom's
 * replicated movement/transform.
 *
 * The component reads flight activity from the broom via
 * AAuraBroomVehicle::IsFlightInputActive() and mount state via
 * GetMountedCharacter(), so it depends on the broom's public read API rather
 * than owning that state.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Broom Motion")
class AURA_API UAuraBroomMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraBroomMotionComponent();

	/**
	 * Sets the world yaw the broom will smoothly rotate toward during flight.
	 * Called by the broom's SetFlightTargetYaw (which forwards here) from rider
	 * steer input and the BT follow director, to keep both systems in sync.
	 */
	UFUNCTION(BlueprintCallable, Category = "Broom|Motion")
	void SetTargetYaw(float WorldYaw);

	/**
	 * Per-frame motion update — idle hover followed by yaw smoothing. Called from
	 * the owning broom's Tick so ordering relative to the movement component is
	 * preserved. No-op on clients (writes are server-authoritative).
	 */
	void TickMotion(float DeltaSeconds);

	// --- Idle hover tunables ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover")
	bool bEnableIdleHover = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverBobAmplitude = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverBobFrequency = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverSwayAmplitude = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverSwayFrequency = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverRollAmplitude = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverPitchAmplitude = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.0"))
	float HoverTiltFrequency = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Idle Hover", meta = (ClampMin = "0.1"))
	float HoverSmoothingSpeed = 2.5f;

	/** Degrees/second interp speed used to rotate the broom toward the target yaw. */
	UPROPERTY(EditDefaultsOnly, Category = "Broom|Movement", meta = (ClampMin = "0.1", ToolTip = "Degrees/second interp speed used to rotate the broom toward the movement or camera direction."))
	float YawInterpSpeed = 8.f;

private:
	void UpdateIdleHover(float DeltaSeconds);
	void UpdateFlightYaw(float DeltaSeconds);

	AAuraBroomVehicle* GetBroomOwner() const;

	// Hover runtime state (captured each time the broom transitions to idle).
	FVector IdleHoverBaseLocation = FVector::ZeroVector;
	FRotator IdleHoverBaseRotation = FRotator::ZeroRotator;
	FVector IdleHoverCurrentOffset = FVector::ZeroVector;
	FRotator IdleHoverCurrentRotationOffset = FRotator::ZeroRotator;
	float IdleHoverTimeSeconds = 0.f;
	bool bWasHoveringLastTick = false;

	// Throttle for the hover yield-to-flight log (Log-level).
	float LastHoverLogTime = -1000.f;
	static constexpr float HoverLogInterval = 0.25f;

	// Target yaw the broom interpolates toward while flying (rider or BT follow).
	float FlightTargetYaw = 0.f;
	bool bHasFlightTargetYaw = false;
};