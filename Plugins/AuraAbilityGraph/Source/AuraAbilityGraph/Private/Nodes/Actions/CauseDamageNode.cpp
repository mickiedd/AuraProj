// Copyright Druid Mechanics

#include "Nodes/Actions/CauseDamageNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AuraAbilityGraphLogChannels.h"
#include "AuraAbilityTypes.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"

UAuraAbilityActionTask* UCauseDamageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UCauseDamageTask>(Outer);
}

EAuraAbilityActionStatus UCauseDamageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[CauseDamage] OnStart"));
    if (!Ctx.AvatarActor || !Ctx.AvatarActor->HasAuthority())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[CauseDamage] OnStart skipped: damage is authority-only"));
        return EAuraAbilityActionStatus::Success;
    }

    AActor* TargetActor = Ctx.CursorHit.GetActor();
    if (!TargetActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: no target actor"));
        return EAuraAbilityActionStatus::Failure;
    }
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[CauseDamage] OnStart TargetActor=%s"), *TargetActor->GetName());

    if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
        {
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
            if (Ctx.ASC && TargetASC && Definition->DamageType.IsValid())
            {
                const FVector Direction = (TargetActor->GetActorLocation() - Ctx.AvatarActor->GetActorLocation()).GetSafeNormal();
                FDamageEffectParams Params;
                Definition->BuildDamageEffectParams(Params, Ctx.ASC, TargetASC, Ctx.AvatarActor, DataAbility->GetAbilityLevel(), Direction);
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[CauseDamage] OnStart applying damage base=%.1f type=%s"), Params.BaseDamage, *Params.DamageType.ToString());
                UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
            }
            else
            {
                UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: missing SourceASC=%s TargetASC=%s DamageTypeValid=%s"),
                    *GetNameSafe(Ctx.ASC), *GetNameSafe(TargetASC), Definition->DamageType.IsValid() ? TEXT("true") : TEXT("false"));
            }
        }
        else
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: no definition"));
        }
    }

    return EAuraAbilityActionStatus::Success;
}
