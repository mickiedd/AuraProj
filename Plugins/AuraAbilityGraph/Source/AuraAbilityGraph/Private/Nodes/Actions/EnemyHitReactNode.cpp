// Copyright Druid Mechanics

#include "Nodes/Actions/EnemyHitReactNode.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "DataAbility.h"
#include "Interaction/CombatInterface.h"

UAuraAbilityActionTask* UEnemyHitReactNode::CreateTask(UObject* Outer) const
{
    return NewObject<UEnemyHitReactTask>(Outer);
}

EAuraAbilityActionStatus UEnemyHitReactTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor || !Ctx.ASC || !Ctx.AvatarActor->Implements<UCombatInterface>())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    UAnimMontage* Montage = ICombatInterface::Execute_GetHitReactMontage(Ctx.AvatarActor);
    if (!Montage)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    ASC = Ctx.ASC;
    HitReactTag = FGameplayTag::RequestGameplayTag(TEXT("Effects.HitReact"), false);
    if (HitReactTag.IsValid() && Ctx.AvatarActor->HasAuthority())
    {
        ASC->AddLooseGameplayTag(HitReactTag);
        bTagAdded = true;
    }

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        OwnerAbility, TEXT("EnemyHitReact"), Montage);
    if (!MontageTask)
    {
        RemoveHitReactTag();
        return EAuraAbilityActionStatus::Failure;
    }
    MontageTask->OnCompleted.AddDynamic(this, &UEnemyHitReactTask::OnCompleted);
    MontageTask->OnBlendOut.AddDynamic(this, &UEnemyHitReactTask::OnCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UEnemyHitReactTask::OnInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &UEnemyHitReactTask::OnInterrupted);
    MontageTask->ReadyForActivation();
    return EAuraAbilityActionStatus::Running;
}

void UEnemyHitReactTask::OnCompleted()
{
    Finish(EAuraAbilityActionStatus::Success);
}

void UEnemyHitReactTask::OnInterrupted()
{
    Finish(EAuraAbilityActionStatus::Failure);
}

void UEnemyHitReactTask::Finish(EAuraAbilityActionStatus Status)
{
    if (bFinishing)
    {
        return;
    }
    bFinishing = true;
    RemoveHitReactTag();
    MontageTask = nullptr;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->AdvanceGraph(Status);
    }
}

void UEnemyHitReactTask::RemoveHitReactTag()
{
    if (bTagAdded && ASC && HitReactTag.IsValid())
    {
        ASC->RemoveLooseGameplayTag(HitReactTag);
    }
    bTagAdded = false;
}

void UEnemyHitReactTask::Cancel(FAuraAbilityExecutionContext& Ctx)
{
    bFinishing = true;
    RemoveHitReactTag();
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }
}
