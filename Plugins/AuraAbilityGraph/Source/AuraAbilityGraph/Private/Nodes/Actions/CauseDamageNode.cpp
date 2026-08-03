// Copyright Druid Mechanics

#include "Nodes/Actions/CauseDamageNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityGraphLogChannels.h"
#include "AuraDamageGameplayEffect.h"

UAuraAbilityActionTask* UCauseDamageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UCauseDamageTask>(Outer);
}

EAuraAbilityActionStatus UCauseDamageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[CauseDamage] OnStart"));
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
            UAbilitySystemComponent* SourceASC = DataAbility->GetAbilitySystemComponentFromActorInfo();
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

            const TSubclassOf<UGameplayEffect> EffectiveGEClass = Definition->DamageEffectClass
                ? TSubclassOf<UGameplayEffect>(Definition->DamageEffectClass)
                : TSubclassOf<UGameplayEffect>(UAuraDamageGameplayEffect::StaticClass());

            if (SourceASC && TargetASC && Definition->DamageType.IsValid())
            {
            	FGameplayEffectSpecHandle SpecHandle = DataAbility->MakeOutgoingGameplayEffectSpec(EffectiveGEClass, DataAbility->GetAbilityLevel());
                if (SpecHandle.IsValid())
                {
                    const float ScaledDamage = Definition->Damage.GetValueAtLevel(DataAbility->GetAbilityLevel());
                    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, Definition->DamageType, ScaledDamage);
                    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
                    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[CauseDamage] OnStart applied damage %.1f type=%s"), ScaledDamage, *Definition->DamageType.ToString());
                }
                else
                {
                    UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: SpecHandle invalid"));
                }
            }
            else
            {
                UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: missing SourceASC=%s TargetASC=%s DamageTypeValid=%s"),
                    *GetNameSafe(SourceASC), *GetNameSafe(TargetASC), Definition->DamageType.IsValid() ? TEXT("true") : TEXT("false"));
            }
        }
        else
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[CauseDamage] OnStart abort: no definition"));
        }
    }

    return EAuraAbilityActionStatus::Success;
}
