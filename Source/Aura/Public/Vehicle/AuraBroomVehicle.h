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

private:
	void UpdateIdleHover(float DeltaSeconds);
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

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastDismountedCharacter;

	float LastDismountServerTime = -1000.f;
};
