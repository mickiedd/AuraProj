// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraBroomMountComponent.generated.h"

class AAuraBroomVehicle;
class ACharacter;
class UPrimitiveComponent;
struct FHitResult;

/**
 * UAuraBroomMountComponent
 *
 * Owns the broom's mount/dismount state machine: the replicated rider reference,
 * collision-driven mounting (OnBroomMeshHit), server RPCs, the remount grace
 * window, character attachment + collision/movement-mode changes, and the
 * Player_Mounted_Broom gameplay tag. Extracted from AAuraBroomVehicle so the
 * rideable-pawn concern stays separate from the mounting-subsystem concern.
 *
 * Replicated: MountedCharacter replicates via this component's
 * GetLifetimeReplicatedProps, and OnRep_MountedCharacter applies the client-side
 * attachment/tag/collision changes. The broom exposes thin forwarders
 * (RequestMount/RequestDismount/GetMountedCharacter/...) that delegate here so
 * external callers (player controller, BT agent, Blueprint) are unchanged.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Broom Mount")
class AURA_API UAuraBroomMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraBroomMountComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Mount a character: authority mounts directly, clients RPC the server. */
	void RequestMount(ACharacter* CharacterToMount);

	/** Dismount the current rider: authority dismounts directly, clients RPC. */
	void RequestDismount();

	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	ACharacter* GetMountedCharacter() const { return MountedCharacter; }

	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	ACharacter* GetLastDismountedCharacter() const { return LastDismountedCharacter; }

	/**
	 * Seconds elapsed since the most recent dismount, or -1.f if none recorded.
	 * Used by the BT follow director to drive a short back-away-from-player window
	 * after a rider dismounts.
	 */
	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	float GetTimeSinceLastDismount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	bool IsWithinRemountGraceWindow(const ACharacter* Character) const;

	UPROPERTY(ReplicatedUsing = OnRep_MountedCharacter, BlueprintReadOnly, Category = "Broom|Mount")
	TObjectPtr<ACharacter> MountedCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	FName RiderSocketName = FName("RiderSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount")
	FName DismountSocketName = FName("DismountSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Broom|Mount", meta = (ClampMin = "0.0"))
	float RemountGracePeriodSeconds = 0.75f;

private:
	AAuraBroomVehicle* GetBroomOwner() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastMountedCharacter;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> LastDismountedCharacter;

	float LastDismountServerTime = -1000.f;
};