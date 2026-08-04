// Copyright Druid Mechanics

#include "Nodes/Actions/WaitForTargetDataNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

bool UWaitForTargetDataNode::IsValidTargetData(const FGameplayAbilityTargetDataHandle& DataHandle, const AActor* AvatarActor, float MaxTargetDistance, FString& OutReason)
{
    if (DataHandle.Num() == 0)
    {
        OutReason = TEXT("target data handle is empty");
        return false;
    }

    const FGameplayAbilityTargetData* TargetData = DataHandle.Get(0);
    const FHitResult* HitResult = TargetData ? TargetData->GetHitResult() : nullptr;
    if (!HitResult)
    {
        OutReason = TEXT("target data has no hit result");
        return false;
    }

    if (!HitResult->bBlockingHit)
    {
        OutReason = TEXT("target data is not a blocking hit");
        return false;
    }

    const FVector TargetLocation = HitResult->ImpactPoint.IsNearlyZero()
        ? HitResult->Location
        : HitResult->ImpactPoint;
    if (TargetLocation.ContainsNaN())
    {
        OutReason = TEXT("target location is not finite");
        return false;
    }

    if (!AvatarActor)
    {
        OutReason = TEXT("avatar actor is unavailable");
        return false;
    }

    if (AActor* TargetActor = HitResult->GetActor())
    {
        if (!IsValid(TargetActor))
        {
            OutReason = TEXT("target actor is invalid");
            return false;
        }
        if (TargetActor == AvatarActor)
        {
            OutReason = TEXT("target actor is the avatar itself");
            return false;
        }
    }

    const float DistanceSquared = FVector::DistSquared(AvatarActor->GetActorLocation(), TargetLocation);
    if (DistanceSquared > FMath::Square(MaxTargetDistance))
    {
        OutReason = FString::Printf(TEXT("target is %.1f units away (maximum %.1f)"), FMath::Sqrt(DistanceSquared), MaxTargetDistance);
        return false;
    }

    return true;
}

UAuraAbilityActionTask* UWaitForTargetDataNode::CreateTask(UObject* Outer) const
{
    return NewObject<UWaitForTargetDataTask>(Outer);
}

void UWaitForTargetDataNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("MaxTargetDistance"))
        {
            MaxTargetDistance = FCString::Atof(*Property.Value);
        }
    }
}

EAuraAbilityActionStatus UWaitForTargetDataTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForTargetData] OnStart OwnerAbility=%s ASC=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.ASC));
    if (!OwnerAbility || !Ctx.ASC)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnStart abort: missing OwnerAbility or ASC"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UWaitForTargetDataNode* Node = Cast<UWaitForTargetDataNode>(NodeDef);
    if (!Node || !Ctx.AvatarActor || !FMath::IsFinite(Node->MaxTargetDistance) || Node->MaxTargetDistance <= 0.f)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnStart abort: invalid avatar or MaxTargetDistance"));
        return EAuraAbilityActionStatus::Failure;
    }

    UTargetDataUnderMouse* Task = UTargetDataUnderMouse::CreateTargetDataUnderMouse(OwnerAbility);
    if (!Task)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnStart abort: failed to create TargetDataUnderMouse task"));
        return EAuraAbilityActionStatus::Failure;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UWaitForTargetDataTask, OnValidData));
    Task->ValidData.Add(Delegate);
    Task->ReadyForActivation();

    TargetDataTask = Task;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingTargetDataTask = Task;
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForTargetData] OnStart waiting for cursor target data"));
    return EAuraAbilityActionStatus::Running;
}

void UWaitForTargetDataTask::OnValidData(const FGameplayAbilityTargetDataHandle& DataHandle)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[WaitForTargetData] OnValidData received DataNum=%d"), DataHandle.Num());

    const UWaitForTargetDataNode* Node = Cast<UWaitForTargetDataNode>(NodeDef);
    const FGameplayAbilityActorInfo* ActorInfo = OwnerAbility ? OwnerAbility->GetCurrentActorInfo() : nullptr;
    const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    FString ValidationReason;
    if (!Node || !UWaitForTargetDataNode::IsValidTargetData(DataHandle, AvatarActor, Node->MaxTargetDistance, ValidationReason))
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnValidData rejected client target: %s"), *ValidationReason);
        PendingStatus = EAuraAbilityActionStatus::Failure;
        if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Failure);
        }
        return;
    }

    PendingStatus = EAuraAbilityActionStatus::Success;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->OnTargetDataReady(DataHandle);
    }
}

void UWaitForTargetDataTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForTargetData] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
    if (TargetDataTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForTargetData] OnExit ending target data task"));
        TargetDataTask->EndTask();
        TargetDataTask.Reset();
    }
}
