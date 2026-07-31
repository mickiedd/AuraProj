// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/CombatInterface.h"
#include "TestCombatAvatar.generated.h"

class USphereComponent;

/**
 * Minimal test-only actor that implements ICombatInterface so the data-driven spawn
 * nodes (SpawnProjectiles, ElectrocuteBeam, ...) — which call
 * ICombatInterface::Execute_GetCombatSocketLocation — can be driven headlessly in a
 * smoke test without spawning a full AAuraCharacterBase (which needs a mesh/weapon).
 *
 * GetCombatSocketLocation_Implementation returns a deterministic, settable offset from
 * the actor's location so the test knows the exact expected socket world position.
 * The remaining ICombatInterface surface is stubbed (the spawn nodes never touch it).
 */
UCLASS(Blueprintable)
class AURAABILITYGRAPH_API ATestCombatAvatar : public AActor, public ICombatInterface
{
    GENERATED_BODY()

public:
    ATestCombatAvatar();

    // Root collision sphere so a second instance can act as a trace target for
    // ElectrocuteBeam's SphereTraceSingle (Visibility channel). Radius is settable.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
    USphereComponent* CollisionSphere;

    // Configurable socket offset; GetCombatSocketLocation returns ActorLocation + this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FVector SocketOffset = FVector(0.f, 0.f, 50.f);

    // Sphere radius for the collision component (set before spawning/registration).
    float SphereRadius = 50.f;

    virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;

    // --- ICombatInterface pure-virtual stubs (unused by spawn nodes) ---
    virtual void Die(const FVector& DeathImpulse) override {}
    virtual FOnDeathSignature& GetOnDeathDelegate() override { return OnDeathDelegate; }
    virtual FOnDamageSignature& GetOnDamageSignature() override { return OnDamageSignature; }
    virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override { return OnASCRegistered; }

private:
    UPROPERTY()
    FOnDeathSignature OnDeathDelegate;

    FOnDamageSignature OnDamageSignature;
    FOnASCRegistered OnASCRegistered;
};