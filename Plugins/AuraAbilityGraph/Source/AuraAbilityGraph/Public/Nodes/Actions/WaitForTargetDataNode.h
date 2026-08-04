// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "WaitForTargetDataNode.generated.h"

class UTargetDataUnderMouse;

UCLASS(DisplayName = "WaitForTargetData")
class AURAABILITYGRAPH_API UWaitForTargetDataNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    /**
     * Validates client-supplied target data before the graph consumes it. Rejects
     * empty handles, missing/non-blocking hits, non-finite locations, invalid or
     * self-targets, and out-of-range targets. Static so the smoke suite can
     * regression-test each rejection branch directly.
     */
    static bool IsValidTargetData(const FGameplayAbilityTargetDataHandle& DataHandle, const AActor* AvatarActor, float MaxTargetDistance, FString& OutReason);

    // Target data is supplied by the client, so the received hit must be checked
    // against the avatar before the graph consumes it.  The authored abilities
    // currently use the default 10,000-unit range; XML can override it per node.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaitForTargetData", meta = (ClampMin = "1.0"))
    float MaxTargetDistance = 10000.f;
};

UCLASS()
class AURAABILITYGRAPH_API UWaitForTargetDataTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;

private:
    UFUNCTION()
    void OnValidData(const FGameplayAbilityTargetDataHandle& DataHandle);

    TWeakObjectPtr<UTargetDataUnderMouse> TargetDataTask;
};
