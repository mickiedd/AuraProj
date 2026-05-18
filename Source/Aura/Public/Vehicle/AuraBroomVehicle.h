// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AuraBroomVehicle.generated.h"

class UFloatingPawnMovement;
class UStaticMeshComponent;
class USceneComponent;
class ACharacter;

UCLASS(Blueprintable)
class AURA_API AAuraBroomVehicle : public APawn
{
	GENERATED_BODY()

public:
	AAuraBroomVehicle();

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

	void MountCharacterInternal(ACharacter* CharacterToMount);
	void DismountCharacterInternal();
	void ApplyMountedState(ACharacter* Character, bool bIsMounted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UStaticMeshComponent> BroomMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UFloatingPawnMovement> FlightMovement;

	UPROPERTY(ReplicatedUsing = OnRep_MountedCharacter, BlueprintReadOnly, Category = "Broom|Mount")
	TObjectPtr<ACharacter> MountedCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	FName RiderSocketName = FName("RiderSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	bool bPossessOnMount = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	bool bRestoreCharacterControlOnDismount = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastMountedCharacter;
};
