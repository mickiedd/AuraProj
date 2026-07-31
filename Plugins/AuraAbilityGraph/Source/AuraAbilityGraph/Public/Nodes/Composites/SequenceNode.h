// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "SequenceNode.generated.h"

class UAuraAbilityActionTask;

UCLASS(DisplayName = "Sequence")
class AURAABILITYGRAPH_API UAuraSequenceNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAABILITYGRAPH_API UAuraSequenceTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
    // UAuraDataAbility::EndAbility calls RootTask->Cancel() to tear down a graph that is
    // still Running (e.g. a channeled ability ended by input release). The base Cancel is
    // a no-op, so without this override running children — a beam's Niagara arc + tick
    // timer, a wait's delay handle — are never notified and leak until PIE world teardown.
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;
};
