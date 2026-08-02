// Copyright Druid Mechanics

#include "Nodes/Actions/EnemyCombatMontageNode.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DataAbility.h"
#include "Interaction/CombatInterface.h"
#include "Interaction/EnemyInterface.h"
#include "TimerManager.h"

UAuraAbilityActionTask* UEnemyCombatMontageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UEnemyCombatMontageTask>(Outer);
}

void UEnemyCombatMontageNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("Timeout"))
        {
            Timeout = FCString::Atof(*Property.Value);
        }
    }
}

EAuraAbilityActionStatus UEnemyCombatMontageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor ||
        !Ctx.AvatarActor->Implements<UEnemyInterface>() || !Ctx.AvatarActor->Implements<UCombatInterface>())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    AActor* Target = IEnemyInterface::Execute_GetCombatTarget(Ctx.AvatarActor);
    if (!IsValid(Target))
    {
        return EAuraAbilityActionStatus::Failure;
    }

    Ctx.CursorHit = FHitResult(Target, nullptr, Target->GetActorLocation(), FVector::ZeroVector);
    ICombatInterface::Execute_UpdateFacingTarget(Ctx.AvatarActor, Target->GetActorLocation());

    const TArray<FTaggedMontage> Montages = ICombatInterface::Execute_GetAttackMontages(Ctx.AvatarActor);
    if (Montages.IsEmpty())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const FTaggedMontage& Selection = Montages[FMath::RandRange(0, Montages.Num() - 1)];
    if (!Selection.Montage || !Selection.MontageTag.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }
    Ctx.CombatSocketTag = Selection.SocketTag;

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        OwnerAbility, TEXT("EnemyCombatMontage"), Selection.Montage);
    EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        OwnerAbility, Selection.MontageTag, nullptr, false, true);
    if (!MontageTask || !EventTask)
    {
        Cleanup();
        return EAuraAbilityActionStatus::Failure;
    }

    MontageTask->OnInterrupted.AddDynamic(this, &UEnemyCombatMontageTask::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &UEnemyCombatMontageTask::OnMontageInterrupted);
    EventTask->EventReceived.AddDynamic(this, &UEnemyCombatMontageTask::OnEventReceived);
    MontageTask->ReadyForActivation();
    EventTask->ReadyForActivation();

    const float Timeout = CastChecked<UEnemyCombatMontageNode>(NodeDef)->Timeout;
    if (Timeout > 0.f && OwnerAbility->GetWorld())
    {
        OwnerAbility->GetWorld()->GetTimerManager().SetTimer(
            TimeoutHandle, this, &UEnemyCombatMontageTask::OnTimeout, Timeout, false);
    }
    return EAuraAbilityActionStatus::Running;
}

void UEnemyCombatMontageTask::OnEventReceived(FGameplayEventData EventData)
{
    PendingStatus = EAuraAbilityActionStatus::Success;
    Cleanup();
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
    }
}

void UEnemyCombatMontageTask::OnMontageInterrupted()
{
    Cleanup();
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Failure);
    }
}

void UEnemyCombatMontageTask::OnTimeout()
{
    OnMontageInterrupted();
}

void UEnemyCombatMontageTask::Cleanup()
{
    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TimeoutHandle);
    }
    if (EventTask)
    {
        EventTask->EndTask();
        EventTask = nullptr;
    }
    MontageTask = nullptr;
}

void UEnemyCombatMontageTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    Cleanup();
}

void UEnemyCombatMontageTask::Cancel(FAuraAbilityExecutionContext& Ctx)
{
    if (MontageTask)
    {
        MontageTask->EndTask();
    }
    Cleanup();
}
