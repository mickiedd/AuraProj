// Copyright Druid Mechanics

#include "Nodes/Actions/HitscanTraceNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AuraAbilityTypes.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interaction/CombatInterface.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UHitscanTraceNode::CreateTask(UObject* Outer) const
{
    return NewObject<UHitscanTraceTask>(Outer);
}

void UHitscanTraceNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("SocketTag"))
        {
            SocketTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("TraceRange"))
        {
            TraceRange = FCString::Atof(*Property.Value);
        }
    }
}

EAuraAbilityActionStatus UHitscanTraceTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[HitscanTrace] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[HitscanTrace] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UHitscanTraceNode* Node = Cast<UHitscanTraceNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[HitscanTrace] OnStart abort: NodeDef is not UHitscanTraceNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[HitscanTrace] OnStart SocketTag=%s SocketLoc=%s"), *Node->SocketTag.ToString(), *SocketLocation.ToString());
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * Node->TraceRange;
    }
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[HitscanTrace] OnStart Target=%s TraceRange=%.1f"), *TargetLocation.ToString(), Node->TraceRange);

    const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();
    const FVector TraceEnd = SocketLocation + Direction * Node->TraceRange;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Ctx.AvatarActor);

    const bool bHit = Ctx.AvatarActor->GetWorld()->LineTraceSingleByChannel(Hit, SocketLocation, TraceEnd, ECC_Visibility, Params);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[HitscanTrace] OnStart LineTrace bHit=%s HitActor=%s"), bHit ? TEXT("true") : TEXT("false"), *GetNameSafe(bHit ? Hit.GetActor() : nullptr));

    if (bHit && Hit.GetActor())
    {
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());
        if (TargetASC)
        {
            if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
            {
                FDamageEffectParams DamageParams;
                Definition->BuildDamageEffectParams(DamageParams, Ctx.ASC, TargetASC, Ctx.AvatarActor, OwnerAbility->GetAbilityLevel(), Direction);
                UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[HitscanTrace] OnStart applying damage base=%.1f type=%s"), DamageParams.BaseDamage, *DamageParams.DamageType.ToString());
                UAuraAbilitySystemLibrary::ApplyDamageEffect(DamageParams);
            }
        }
        else
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[HitscanTrace] OnStart hit actor has no ASC"));
        }
    }

    return EAuraAbilityActionStatus::Success;
}
