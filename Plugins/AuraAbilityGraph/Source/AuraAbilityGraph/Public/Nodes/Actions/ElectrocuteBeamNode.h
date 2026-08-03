// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "ElectrocuteBeamNode.generated.h"

class UNiagaraComponent;

/**
 * ElectrocuteBeam node — encapsulates the Electrocute channeled beam ability.
 *
 * Uses the cursor target data to find the primary target, with a forward
 * sphere-trace fallback when no cursor hit is available. World geometry can be
 * used as a visual-only endpoint, while combat actors receive damage and chain
 * lightning.
 * up to MaxChainTargets. Sets up a repeating timer that ticks damage on all
 * targets every TickInterval seconds. The beam ends after ChannelDuration (or
 * early if all targets die), so the ability completes and can be re-triggered.
 *
 * The beam visual (NS_ElectricBeam) is spawned as a UNiagaraComponent on every
 * machine — server + casting client — so the lightning arc is visible. Each
 * component's BeamStart/BeamEnd Niagara variables are driven to the weapon socket
 * and the trace impact point. On the server the endpoints are refreshed every
 * damage tick so the arc tracks moving enemies; on the client the arc is drawn
 * once at spawn and cleaned up when the ability ends.
 *
 * XML properties:
 *   SocketTag          — combat socket to trace from (default: CombatSocket.Weapon)
 *   TraceRadius        — sphere trace radius from socket to target (default: 10)
 *   MaxChainTargets    — max additional chain targets (default: 5)
 *   ChainRadius         — radius to search for chain targets (default: 850)
 *   TickInterval       — seconds between damage ticks (default: 0.2)
 *   ChannelDuration    — seconds the beam channels before ending the ability on
 *                        its own (default: 2.0). Without a finite duration the beam
 *                        returns Running forever and the ability never ends, so it
 *                        can't be re-triggered. The beam still ends early if all
 *                        targets die first.
 *   BeamEffect         — NiagaraSystem asset path for the beam arc (optional)
 *   BeamStartParameter — Niagara variable name for the beam source (default: User.Beam Start)
 *   BeamEndParameter    — Niagara variable name for the beam target (default: User.Beam End)
 */
UCLASS(DisplayName = "ElectrocuteBeam")
class AURAABILITYGRAPH_API UElectrocuteBeamNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    FGameplayTag SocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float TraceRadius = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float MaxBeamRange = 3000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    int32 MaxChainTargets = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float ChainRadius = 850.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float TickInterval = 0.2f;

    // Seconds the beam channels before ending the ability on its own. 0 = channel
    // forever (legacy behavior; leaves the ability un-retriggerable). The beam ends
    // early if all targets die first.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float ChannelDuration = 2.0f;

    // NiagaraSystem asset path. Defaults to the project's shock beam so the ability
    // renders out-of-the-box even if the XML omits it (the bug that kept re-breaking
    // the visual when the ElectrocuteBeam node's properties were stripped). Set to
    // empty in XML to explicitly disable the visual.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    FString BeamEffect = TEXT("/Game/Assets/Effects/Shock/NS_ElectricBeam.NS_ElectricBeam");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    FString BeamStartParameter = TEXT("User.Beam Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    FString BeamEndParameter = TEXT("User.Beam End");
};

// One beam target paired with the Niagara arc component drawn to it.
USTRUCT()
struct FAuraBeamTarget
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> Actor;

    // Desired world-space end point of this beam arc. For a real enemy this is the
    // trace impact point (on the body) and is refreshed to the actor's current
    // location each tick so the arc tracks movement. For a non-enemy hit (ground/
    // wall) it stays at the impact point so the beam points where the cursor aimed
    // instead of at the hit actor's pivot (which for landscape is the world origin).
    UPROPERTY()
    FVector BeamEndLocation = FVector::ZeroVector;

    // A cursor point on world geometry has no actor to damage, but must remain
    // alive for the duration of the visual channel. This is also used for the
    // deterministic open-sky fallback target.
    UPROPERTY()
    bool bVisualOnly = false;

    UPROPERTY()
    bool bDamageTarget = false;

    // WEAK on purpose: a strong UPROPERTY ref here would root the NiagaraComponent,
    // and (via the task -> ability -> ASC -> PlayerState -> World Outer chain) keep it
    // alive past EndPlayMap, tripping the editor's stale-reference ensure / teardown
    // crash. The component is registered with the World, so world teardown destroys it
    // without us holding it alive. See the weak-capture rationale in ElectrocuteBeamNode.cpp.
    TWeakObjectPtr<UNiagaraComponent> Beam;
};

UCLASS()
class AURAABILITYGRAPH_API UElectrocuteBeamTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;

    // Test accessor: lets the smoke test assert Cancel/OnExit cleared beam state
    // without friending it or exposing the array.
    int32 GetBeamTargetCountForTest() const { return BeamTargets.Num(); }

    // Test accessor: true if any beam target has a live UNiagaraComponent (the arc was
    // actually spawned), so the smoke test can assert "effect spawned".
    bool HasBeamComponentForTest() const;

    // Test accessor: the desired world-space beam end point for target Index (zero if
    // out of range), so the smoke test can assert "spawned to the right location".
    FVector GetBeamEndLocationForTest(int32 Index) const;

    // Test accessor: the world-space launch point currently used by the beam (zero if
    // the task has no valid avatar/socket). This guards the muzzle-side endpoint.
    FVector GetBeamStartLocationForTest() const;

    // Test accessor: the Niagara component transform. NS_ElectricBeam consumes absolute
    // world positions, so its component must remain at world origin even when the muzzle
    // is elsewhere in the level.
    FVector GetBeamComponentLocationForTest(int32 Index) const;

private:
    FTimerHandle TickTimerHandle;
    FTimerHandle ChannelTimerHandle;
    TArray<FAuraBeamTarget> BeamTargets;
    FAuraAbilityExecutionContext CachedCtx;

    void TickDamage();
    void FindBeamTargets(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node);
    void SpawnBeamFX(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node);
    void RefreshBeamFX(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node);
    void CleanupBeams();
};
