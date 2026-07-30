// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "ElectrocuteBeamNode.generated.h"

/**
 * ElectrocuteBeam node — encapsulates the Electrocute channeled beam ability.
 *
 * Traces from the weapon socket to the cursor hit location to find the primary
 * target. Then finds additional nearby targets (chain lightning) up to
 * MaxChainTargets. Sets up a repeating timer that ticks damage on all targets
 * every TickInterval seconds. The beam continues until the ability is cancelled
 * (input release, target death, or manual cancel).
 *
 * XML properties:
 *   SocketTag         — combat socket to trace from (default: CombatSocket.Weapon)
 *   TraceRadius       — sphere trace radius from socket to target (default: 10)
 *   MaxChainTargets   — max additional chain targets (default: 5)
 *   ChainRadius       — radius to search for chain targets (default: 850)
 *   TickInterval      — seconds between damage ticks (default: 0.2)
 *   GameplayCueTag    — gameplay cue tag for the shock effect (optional)
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
    int32 MaxChainTargets = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float ChainRadius = 850.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    float TickInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ElectrocuteBeam")
    FString GameplayCueTag;
};

UCLASS()
class AURAABILITYGRAPH_API UElectrocuteBeamTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;

private:
    FTimerHandle TickTimerHandle;
    TArray<TWeakObjectPtr<AActor>> BeamTargets;
    FAuraAbilityExecutionContext CachedCtx;

    void TickDamage();
    void FindBeamTargets(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node);
};