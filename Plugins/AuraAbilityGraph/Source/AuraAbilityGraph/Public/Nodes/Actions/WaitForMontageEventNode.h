// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "WaitForMontageEventNode.generated.h"

UCLASS(DisplayName = "WaitForMontageEvent")
class AURAABILITYGRAPH_API UWaitForMontageEventNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaitForMontageEvent")
    FGameplayTag EventTag;

    // Seconds to wait for the montage gameplay event before giving up and ending the
    // ability as cancelled. 0 = wait forever (legacy behavior — hangs the ability
    // Running if the AnimNotify is missing or the event is swallowed, e.g. ArcaneShards
    // stuck active for 84s until PIE teardown). A small value (e.g. 5) is a safety net:
    // the cast montage event fires within a fraction of a second in normal play, so this
    // only trips when something is genuinely wrong, and the ability ends instead of
    // hanging un-retriggerable.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaitForMontageEvent")
    float Timeout = 0.f;

    // Dedicated servers do not always evaluate the locally-authored montage notify.
    // A positive value lets an authority advance this wait after the cast presentation
    // window, while clients continue to use the actual montage event. 0 disables the
    // authority fallback for abilities whose server montage event is reliable.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaitForMontageEvent")
    float AuthorityFallbackDelay = 0.f;
};

UCLASS()
class AURAABILITYGRAPH_API UWaitForMontageEventTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;

private:
    UFUNCTION()
    void OnEventReceived(FGameplayEventData EventData);

    // Fired by TimeoutHandle when the wait has gone on too long — advances the graph
    // with Failure so the ability ends (cancelled) instead of hanging Running forever.
    void OnTimeout();

    // Advances the authoritative graph when the local montage event is unavailable on a
    // dedicated server, allowing authority-only actions such as projectile spawning to run.
    void OnAuthorityFallback();

    FTimerHandle TimeoutHandle;
    FTimerHandle AuthorityFallbackHandle;
};
