// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Pawn.h"
#include "AuraBroomVehicle.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPrimitiveComponent;
class ACharacter;
class UAuraBroomAgentComponent;
struct FHitResult;

/**
 * Server-authoritative movement component for the broom.
 * UFloatingPawnMovement normally gates movement on Controller->IsLocalController(),
 * which prevents movement when the broom has no possessing controller.
 * This subclass skips that check and drives movement from server authority instead.
 */
UCLASS()
class AURA_API UAuraBroomMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Re-injects the BT follow thrust as standard movement input every tick so the
	// broom continuously accelerates toward the player. The BT only fires
	// AddFlightInput ~5-10 Hz; UFloatingPawnMovement consumes input each tick, so
	// without re-injection the high Deceleration kills velocity between pulses and
	// the broom crawls (~10 cm/s) instead of flying.
	virtual void ApplyControlInputToVelocity(float DeltaTime) override;

private:
	// Throttle for the diagnostic move log (Log-level, so it shows in cooked server logs).
	float LastMoveLogTime = -1000.f;
};

UCLASS(Blueprintable)
class AURA_API AAuraBroomVehicle : public APawn
{
	GENERATED_BODY()

public:
	AAuraBroomVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void AddFlightInput(const FVector& WorldDirection, float ScaleValue = 1.f);

	/**
	 * Sets the persistent world-space flight thrust the BT uses to follow a player.
	 * Unlike AddFlightInput (one-shot, consumed each tick), this thrust is re-applied
	 * every movement tick so the broom continuously accelerates between the BT's
	 * ~5-10 Hz pulses. Pass FVector::ZeroVector to stop (coast/decelerate).
	 */
	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void SetBtFlightThrust(const FVector& WorldThrust) { BtFlightThrust = WorldThrust; }

	UFUNCTION(BlueprintPure, Category = "Broom|Movement")
	FVector GetBtFlightThrust() const { return BtFlightThrust; }

	/**
	 * Sets the yaw the broom will smoothly rotate toward during flight.
	 * Called by movement input (A/D/S) and by the camera-rotation path to keep both
	 * systems in sync and prevent them from fighting each other.
	 */
	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void SetFlightTargetYaw(float WorldYaw);

	UFUNCTION(BlueprintCallable, Category = "Broom|Mount")
	void RequestMount(ACharacter* CharacterToMount);

	UFUNCTION(BlueprintCallable, Category = "Broom|Mount")
	void RequestDismount();

	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	ACharacter* GetMountedCharacter() const { return MountedCharacter; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestMount(ACharacter* CharacterToMount);

	UFUNCTION(Server, Reliable)
	void ServerRequestDismount();

	UFUNCTION()
	void OnRep_MountedCharacter();

	UFUNCTION()
	void OnBroomMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void MountCharacterInternal(ACharacter* CharacterToMount);
	void DismountCharacterInternal();
	void ApplyMountedState(ACharacter* Character, bool bIsMounted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UStaticMeshComponent> BroomMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomMovement> FlightMovement;

	UPROPERTY(ReplicatedUsing = OnRep_MountedCharacter, BlueprintReadOnly, Category = "Broom|Mount")
	TObjectPtr<ACharacter> MountedCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	FName RiderSocketName = FName("RiderSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	FName DismountSocketName = FName("DismountSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount", meta = (ClampMin = "0.0"))
	float RemountGracePeriodSeconds = 0.75f;

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

	UPROPERTY(EditDefaultsOnly, Category = "Broom|Movement", meta = (ClampMin = "0.1", ToolTip = "Degrees/second interp speed used to rotate the broom toward the movement or camera direction."))
	float YawInterpSpeed = 8.f;

	/** BehaviorU (Behaviac) agent driving the broom follow behavior tree (BT_BroomFollowPlayer.xml). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|AI")
	TObjectPtr<UAuraBroomAgentComponent> BroomAgentComponent;

private:
	void UpdateIdleHover(float DeltaSeconds);
	void UpdateFlightYaw(float DeltaSeconds);
	bool IsWithinRemountGraceWindow(const ACharacter* Character) const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastMountedCharacter;

	FVector IdleHoverBaseLocation = FVector::ZeroVector;
	FRotator IdleHoverBaseRotation = FRotator::ZeroRotator;
	FVector IdleHoverCurrentOffset = FVector::ZeroVector;
	FRotator IdleHoverCurrentRotationOffset = FRotator::ZeroRotator;
	float IdleHoverTimeSeconds = 0.f;
	bool bWasHoveringLastTick = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug|Movement", meta = (ClampMin = "0.1"))
	float FlightInputLogInterval = 0.25f;

	float LastFlightInputLogTime = -1000.f;
	float LastFlightBlockedLogTime = -1000.f;
	float LastHoverLogTime = -1000.f;
	// Server time of the most recent AddFlightInput call, so UpdateIdleHover can
	// keep yielding to flight between BT input pulses (the BT fires ~10 Hz).
	float LastFlightInputAppliedTime = -1000.f;

	// Target yaw the broom interpolates toward while the player is mounted and flying.
	float FlightTargetYaw = 0.f;
	bool bHasFlightTargetYaw = false;

	// Persistent world-space thrust (direction * scale) commanded by the BT follow
	// behavior. Re-injected into the movement component every tick so the broom
	// keeps flying between BT pulses. Zero = stop/coast.
	FVector BtFlightThrust = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastDismountedCharacter;

	float LastDismountServerTime = -1000.f;
};
