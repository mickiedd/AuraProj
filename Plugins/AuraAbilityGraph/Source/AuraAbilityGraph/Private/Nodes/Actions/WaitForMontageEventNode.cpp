// Copyright Druid Mechanics

#include "Nodes/Actions/WaitForMontageEventNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "Nodes/Actions/PlayMontageNode.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

static const UPlayMontageNode* FindPlayMontageNode(const UAuraAbilityActionNode* Node)
{
    if (!Node)
    {
        return nullptr;
    }

    if (const UPlayMontageNode* PlayMontageNode = Cast<UPlayMontageNode>(Node))
    {
        return PlayMontageNode;
    }

    for (const UAuraAbilityActionNode* Child : Node->Children)
    {
        if (const UPlayMontageNode* PlayMontageNode = FindPlayMontageNode(Child))
        {
            return PlayMontageNode;
        }
    }

    return nullptr;
}

static bool MontageContainsGameplayEventTag(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
    if (!Montage)
    {
        return false;
    }

    for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
    {
        const UObject* NotifyObject = NotifyEvent.Notify
            ? static_cast<const UObject*>(NotifyEvent.Notify)
            : static_cast<const UObject*>(NotifyEvent.NotifyStateClass);
        if (!NotifyObject)
        {
            continue;
        }

        const FStructProperty* EventTagProperty = CastField<FStructProperty>(
            NotifyObject->GetClass()->FindPropertyByName(TEXT("EventTag")));
        if (!EventTagProperty || EventTagProperty->Struct != FGameplayTag::StaticStruct())
        {
            continue;
        }

        const FGameplayTag* AuthoredTag = EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(NotifyObject);
        if (AuthoredTag && *AuthoredTag == EventTag)
        {
            return true;
        }
    }

    return false;
}

UAuraAbilityActionTask* UWaitForMontageEventNode::CreateTask(UObject* Outer) const
{
    return NewObject<UWaitForMontageEventTask>(Outer);
}

void UWaitForMontageEventNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("EventTag"))
        {
            EventTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("Timeout"))
        {
            Timeout = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("AuthorityFallbackDelay"))
        {
            AuthorityFallbackDelay = FCString::Atof(*Property.Value);
        }
    }
}

bool UWaitForMontageEventTask::TryClaimCompletion()
{
    if (bCompletionClaimed)
    {
        return false;
    }

    bCompletionClaimed = true;
    return true;
}

void UWaitForMontageEventTask::ClearWaitTimers()
{
    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TimeoutHandle);
        World->GetTimerManager().ClearTimer(AuthorityFallbackHandle);
    }

    TimeoutHandle.Invalidate();
    AuthorityFallbackHandle.Invalidate();
}

EAuraAbilityActionStatus UWaitForMontageEventTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    bCompletionClaimed = false;
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart OwnerAbility=%s"), *GetNameSafe(OwnerAbility));
    if (!OwnerAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: OwnerAbility null"));
        return EAuraAbilityActionStatus::Failure;
    }

    FGameplayTag EventTag = NodeDef ? Cast<UWaitForMontageEventNode>(NodeDef)->EventTag : FGameplayTag();
    if (!EventTag.IsValid())
    {
        // The node's own EventTag is now the only source (the ability-level <montage
        // eventTag=...> fallback was removed). An empty tag would WaitGameplayEvent on
        // FGameplayTag() and never fire — fail fast so the config error surfaces instead
        // of hanging the ability Running until the Timeout (or forever if Timeout=0).
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: no EventTag set on node"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UAuraAbilityDefinition* Definition = Ctx.Definition;
    if (!Definition)
    {
        if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            Definition = DataAbility->GetDefinition();
        }
    }

    const UPlayMontageNode* PlayMontageNode = Definition ? FindPlayMontageNode(Definition->RootNode) : nullptr;
    if (!PlayMontageNode || !PlayMontageNode->Montage)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: no authored montage found for EventTag=%s"), *EventTag.ToString());
        return EAuraAbilityActionStatus::Failure;
    }

    if (!MontageContainsGameplayEventTag(PlayMontageNode->Montage, EventTag))
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: EventTag=%s is not authored on montage %s"), *EventTag.ToString(), *GetNameSafe(PlayMontageNode->Montage));
        return EAuraAbilityActionStatus::Failure;
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart EventTag=%s"), *EventTag.ToString());

    UAbilityTask_WaitGameplayEvent* Task = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(OwnerAbility, EventTag, nullptr, false, true);
    if (!Task)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: WaitGameplayEvent returned null"));
        return EAuraAbilityActionStatus::Failure;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UWaitForMontageEventTask, OnEventReceived));
    Task->EventReceived.Add(Delegate);

    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingMontageEventTask = Task;
    }

    Task->ReadyForActivation();

    // Safety net: if the montage gameplay event never arrives (missing AnimNotify on
    // the cast montage, event swallowed, or the montage didn't play) the wait would
    // otherwise hang the ability Running forever — un-retriggerable, and the stuck
    // graph is what later crashed PIE teardown. A Timeout > 0 arms a one-shot timer
    // that gives up and cancels the wait. The cast-montage event fires in a fraction
    // of a second in normal play, so this only trips when something is genuinely wrong.
    const UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(NodeDef);
    const float TimeoutSec = EventNode ? EventNode->Timeout : 0.f;
    const float AuthorityFallbackDelaySec = EventNode ? EventNode->AuthorityFallbackDelay : 0.f;
    if (TimeoutSec > 0.f && OwnerAbility)
    {
        if (UWorld* World = OwnerAbility->GetWorld())
        {
            FTimerDelegate TimeoutDel;
            // Weak captures (ability + task) so a PIE teardown mid-wait can't root the
            // task via the timer delegate — same rationale as the ElectrocuteBeam timers.
            TimeoutDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UWaitForMontageEventTask>(this)]()
            {
                if (UWaitForMontageEventTask* Task = ThisObj.Get())
                {
                    Task->OnTimeout();
                }
            });
            World->GetTimerManager().SetTimer(TimeoutHandle, TimeoutDel, TimeoutSec, false);
        }
    }

    // Montage gameplay events are generated locally. On a dedicated server the
    // montage can be valid and the client can observe the notify while the server
    // never evaluates it. Let the ability definition opt into a short authority
    // fallback so authority-only gameplay actions are not blocked by presentation.
    if (AuthorityFallbackDelaySec > 0.f
        && Ctx.AvatarActor
        && Ctx.AvatarActor->HasAuthority()
        && OwnerAbility)
    {
        if (UWorld* World = OwnerAbility->GetWorld())
        {
            FTimerDelegate FallbackDel;
            FallbackDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UWaitForMontageEventTask>(this)]()
            {
                if (UWaitForMontageEventTask* Task = ThisObj.Get())
                {
                    Task->OnAuthorityFallback();
                }
            });
            World->GetTimerManager().SetTimer(
                AuthorityFallbackHandle,
                FallbackDel,
                AuthorityFallbackDelaySec,
                false);
        }
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart waiting for event (timeout=%.2f authorityFallback=%.2f)"), TimeoutSec, AuthorityFallbackDelaySec);
    return EAuraAbilityActionStatus::Running;
}

void UWaitForMontageEventTask::OnTimeout()
{
	if (!TryClaimCompletion())
	{
		return;
	}

    // Event never arrived in time. Clear our handle and advance with Failure so the
    // Sequence ends this node and the DataAbility ends as cancelled instead of hanging.
    ClearWaitTimers();
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] Timed out waiting for montage event — ending ability as cancelled"));
        if (DataAbility->PendingMontageEventTask.IsValid())
        {
            DataAbility->PendingMontageEventTask->EndTask();
        }
        DataAbility->PendingMontageEventTask.Reset();
        DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Failure);
    }
}

void UWaitForMontageEventTask::OnAuthorityFallback()
{
    if (!TryClaimCompletion())
    {
		return;
	}

    ClearWaitTimers();
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForMontageEvent] Authority fallback advanced the graph after the montage event was not observed"));
        if (DataAbility->PendingMontageEventTask.IsValid())
        {
            DataAbility->PendingMontageEventTask->EndTask();
            DataAbility->PendingMontageEventTask.Reset();
        }
        PendingStatus = EAuraAbilityActionStatus::Success;
        DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
    }
}

void UWaitForMontageEventTask::OnEventReceived(FGameplayEventData EventData)
{
    if (!TryClaimCompletion())
    {
        return;
    }

    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[WaitForMontageEvent] OnEventReceived received"));
    ClearWaitTimers();
    PendingStatus = EAuraAbilityActionStatus::Success;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        // Route through the canonical UAuraDataAbility::OnMontageEventReceived so the
        // graph advance follows the same bGraphActive lifecycle guard as
        // OnMontageCompleted/OnTargetDataReady (it also resets PendingMontageEventTask).
        // Previously this duplicated the reset + AdvanceGraph and bypassed the guard —
        // a stray event after the graph ended could double-advance a dead graph.
        DataAbility->OnMontageEventReceived(EventData);
    }
}

void UWaitForMontageEventTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
    bCompletionClaimed = true;
    ClearWaitTimers();
}

void UWaitForMontageEventTask::Cancel(FAuraAbilityExecutionContext& Ctx)
{
    bCompletionClaimed = true;
    ClearWaitTimers();
}
