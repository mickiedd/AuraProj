// Copyright Druid Mechanics

#include "Nodes/Actions/EnemyMeleeDamageNode.h"

#include "AbilityDefinition.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityTypes.h"
#include "DataAbility.h"
#include "Interaction/CombatInterface.h"

UAuraAbilityActionTask* UEnemyMeleeDamageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UEnemyMeleeDamageTask>(Outer);
}

void UEnemyMeleeDamageNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("Radius"))
        {
            Radius = FMath::Max(0.f, FCString::Atof(*Property.Value));
        }
    }
}

EAuraAbilityActionStatus UEnemyMeleeDamageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    const UEnemyMeleeDamageNode* MeleeNode = Cast<UEnemyMeleeDamageNode>(NodeDef);
    const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility);
    const UAuraAbilityDefinition* Definition = Ctx.Definition;
    if (!MeleeNode || !DataAbility || !Definition || !Ctx.AvatarActor || !Ctx.ASC)
    {
        return EAuraAbilityActionStatus::Failure;
    }
    if (!Ctx.AvatarActor->HasAuthority())
    {
        return EAuraAbilityActionStatus::Success;
    }

    const FVector Origin = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Ctx.CombatSocketTag);

    TArray<AActor*> Targets;
    UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
        Ctx.AvatarActor, Targets, {Ctx.AvatarActor}, MeleeNode->Radius, Origin);

    for (AActor* Target : Targets)
    {
        if (!IsValid(Target) || !UAuraAbilitySystemLibrary::IsNotFriend(Ctx.AvatarActor, Target))
        {
            continue;
        }
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
        if (!TargetASC)
        {
            continue;
        }

        const FVector Direction = (Target->GetActorLocation() - Origin).GetSafeNormal();
        FDamageEffectParams Params;
        Definition->BuildDamageEffectParams(Params, Ctx.ASC, TargetASC, Ctx.AvatarActor, DataAbility->GetAbilityLevel(), Direction);
        UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
    }
    return EAuraAbilityActionStatus::Success;
}
