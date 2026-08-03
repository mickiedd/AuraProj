// Copyright Druid Mechanics

#include "Nodes/Actions/ApplyDamageNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AuraAbilityGraphLogChannels.h"
#include "AuraAbilityTypes.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"

UAuraAbilityActionTask* UApplyDamageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UApplyDamageTask>(Outer);
}

EAuraAbilityActionStatus UApplyDamageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ApplyDamage] OnStart"));
    AActor* TargetActor = Ctx.CursorHit.GetActor();
    if (!TargetActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ApplyDamage] OnStart abort: no target actor in context"));
        return EAuraAbilityActionStatus::Failure;
    }
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ApplyDamage] OnStart TargetActor=%s"), *TargetActor->GetName());

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!TargetASC)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ApplyDamage] OnStart abort: target has no ASC"));
        return EAuraAbilityActionStatus::Failure;
    }

    if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
        {
            const FVector Direction = (TargetActor->GetActorLocation() - Ctx.AvatarActor->GetActorLocation()).GetSafeNormal();
            FDamageEffectParams Params;
            Params.WorldContextObject = Ctx.AvatarActor;
            Params.SourceAbilitySystemComponent = Ctx.ASC;
            Params.TargetAbilitySystemComponent = TargetASC;
            Params.AbilityLevel = DataAbility->GetAbilityLevel();
            Params.DamageGameplayEffectClass = Definition->DamageEffectClass;
            Params.DamageType = Definition->DamageType;
            Params.BaseDamage = Definition->Damage.GetValueAtLevel(DataAbility->GetAbilityLevel());
            Params.DebuffChance = Definition->DebuffChance;
            Params.DebuffDamage = Definition->DebuffDamage;
            Params.DebuffDuration = Definition->DebuffDuration;
            Params.DebuffFrequency = Definition->DebuffFrequency;
            Params.DeathImpulseMagnitude = Definition->DeathImpulseMagnitude;
            Params.DeathImpulse = Direction * Definition->DeathImpulseMagnitude;
            Params.KnockbackForceMagnitude = Definition->KnockbackForceMagnitude;
            Params.KnockbackForce = Direction * Definition->KnockbackForceMagnitude;
            Params.KnockbackChance = Definition->KnockbackChance;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ApplyDamage] OnStart applying damage base=%.1f type=%s"), Params.BaseDamage, *Params.DamageType.ToString());
            UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
        }
    }

    return EAuraAbilityActionStatus::Success;
}
