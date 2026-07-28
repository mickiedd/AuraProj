// Copyright Druid Mechanics

#include "Nodes/Actions/FaceTargetNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UFaceTargetNode::CreateTask(UObject* Outer) const
{
    return NewObject<UFaceTargetTask>(Outer);
}

void UFaceTargetNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("bSetControllerRotation"))
        {
            bSetControllerRotation = Property.Value.ToBool();
        }
        else if (Property.Name == TEXT("bSetActorRotation"))
        {
            bSetActorRotation = Property.Value.ToBool();
        }
        else if (Property.Name == TEXT("bYawOnly"))
        {
            bYawOnly = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus UFaceTargetTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[FaceTarget] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[FaceTarget] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UFaceTargetNode* Node = Cast<UFaceTargetNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[FaceTarget] OnStart abort: NodeDef is not UFaceTargetNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector AvatarLocation = Ctx.AvatarActor->GetActorLocation();

    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    if (TargetLocation.IsZero())
    {
        TargetLocation = AvatarLocation + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[FaceTarget] OnStart no cursor hit, using fallback target=%s"), *TargetLocation.ToString());
    }

    const FVector Direction = (TargetLocation - AvatarLocation);
    if (Direction.IsZero())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[FaceTarget] OnStart zero direction, skipping rotation"));
        return EAuraAbilityActionStatus::Success;
    }

    FRotator TargetRotation = Direction.Rotation();
    if (Node->bYawOnly)
    {
        TargetRotation.Pitch = 0.f;
        TargetRotation.Roll = 0.f;
    }

    if (Node->bSetControllerRotation)
    {
        if (APawn* ControllablePawn = Cast<APawn>(Ctx.AvatarActor))
        {
            if (AController* Ctrl = ControllablePawn->GetController())
            {
                Ctrl->SetControlRotation(TargetRotation);
            }
        }
    }

    if (Node->bSetActorRotation)
    {
        Ctx.AvatarActor->SetActorRotation(TargetRotation);
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[FaceTarget] OnStart rotated avatar to %s (Controller=%s Actor=%s YawOnly=%s)"),
        *TargetRotation.ToString(),
        Node->bSetControllerRotation ? TEXT("true") : TEXT("false"),
        Node->bSetActorRotation ? TEXT("true") : TEXT("false"),
        Node->bYawOnly ? TEXT("true") : TEXT("false"));
    return EAuraAbilityActionStatus::Success;
}