// Copyright Druid Mechanics

#include "Nodes/Actions/ElectrocuteBeamNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityTypes.h"
#include "Interaction/CombatInterface.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"

UAuraAbilityActionTask* UElectrocuteBeamNode::CreateTask(UObject* Outer) const
{
    return NewObject<UElectrocuteBeamTask>(Outer);
}

void UElectrocuteBeamNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("SocketTag"))
        {
            SocketTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("TraceRadius"))
        {
            TraceRadius = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("MaxChainTargets"))
        {
            MaxChainTargets = FCString::Atoi(*Property.Value);
        }
        else if (Property.Name == TEXT("ChainRadius"))
        {
            ChainRadius = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("TickInterval"))
        {
            TickInterval = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("GameplayCueTag"))
        {
            GameplayCueTag = Property.Value;
        }
    }
}

EAuraAbilityActionStatus UElectrocuteBeamTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));

    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    // Only run on server
    if (!Ctx.AvatarActor->HasAuthority())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] OnStart skipped on non-authority"));
        return EAuraAbilityActionStatus::Success;
    }

    const UElectrocuteBeamNode* Node = Cast<UElectrocuteBeamNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] OnStart abort: NodeDef is not UElectrocuteBeamNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    CachedCtx = Ctx;

    // Find beam targets
    FindBeamTargets(Ctx, Node);

    if (BeamTargets.Num() == 0)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] OnStart no targets found, completing"));
        return EAuraAbilityActionStatus::Success;
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] OnStart found %d targets, starting tick timer interval=%.2f"),
        BeamTargets.Num(), Node->TickInterval);

    // Do first tick immediately
    TickDamage();

    // Set up repeating timer for damage ticks
    UWorld* World = Ctx.AvatarActor->GetWorld();
    if (!World)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    FTimerDelegate TimerDel;
    TimerDel.BindWeakLambda(OwnerAbility, [ThisObj = TStrongObjectPtr<UElectrocuteBeamTask>(this)]()
    {
        if (!ThisObj->OwnerAbility)
        {
            return;
        }
        ThisObj->TickDamage();
    });

    World->GetTimerManager().SetTimer(TickTimerHandle, TimerDel, Node->TickInterval, true);

    // Return Running — the beam continues until the ability is cancelled.
    // The ability ends when EndAbility is called (e.g. on input release by the
    // DataAbility's input handling, or when a montage ends).
    return EAuraAbilityActionStatus::Running;
}

void UElectrocuteBeamTask::FindBeamTargets(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node)
{
    BeamTargets.Empty();

    // Get socket location
    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    // Target location from cursor hit
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
    }

    // Sphere trace from socket to target to find primary target
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Ctx.AvatarActor);

    FHitResult HitResult;
    UKismetSystemLibrary::SphereTraceSingle(
        Ctx.AvatarActor,
        SocketLocation,
        TargetLocation,
        Node->TraceRadius,
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResult,
        true);

    AActor* PrimaryTarget = nullptr;
    if (HitResult.bBlockingHit)
    {
        PrimaryTarget = HitResult.GetActor();
        if (PrimaryTarget)
        {
            BeamTargets.Add(PrimaryTarget);
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Primary target: %s"), *PrimaryTarget->GetName());
        }
    }

    if (!PrimaryTarget)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] No primary target found from trace"));
        return;
    }

    // Find additional chain targets
    const int32 NumAdditional = FMath::Min(
        FMath::Max(0, OwnerAbility->GetAbilityLevel() - 1),
        Node->MaxChainTargets);

    if (NumAdditional <= 0)
    {
        return;
    }

    TArray<AActor*> OverlappingActors;
    ActorsToIgnore.Add(PrimaryTarget);

    UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
        Ctx.AvatarActor,
        OverlappingActors,
        ActorsToIgnore,
        Node->ChainRadius,
        PrimaryTarget->GetActorLocation());

    TArray<AActor*> ClosestTargets;
    UAuraAbilitySystemLibrary::GetClosestTargets(
        NumAdditional,
        OverlappingActors,
        ClosestTargets,
        PrimaryTarget->GetActorLocation());

    for (AActor* Target : ClosestTargets)
    {
        BeamTargets.Add(Target);
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Chain target: %s"), *Target->GetName());
    }
}

void UElectrocuteBeamTask::TickDamage()
{
    if (!OwnerAbility || !CachedCtx.AvatarActor)
    {
        return;
    }

    // Clean up dead/invalid targets
    BeamTargets.RemoveAll([](const TWeakObjectPtr<AActor>& WeakActor)
    {
        return !WeakActor.IsValid();
    });

    if (BeamTargets.Num() == 0)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] All targets dead/invalid, ending beam"));
        if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
        }
        return;
    }

    const UAuraAbilityDefinition* Definition = CachedCtx.Definition;
    if (!Definition)
    {
        return;
    }

    for (const TWeakObjectPtr<AActor>& WeakTarget : BeamTargets)
    {
        AActor* TargetActor = WeakTarget.Get();
        if (!TargetActor)
        {
            continue;
        }

        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (!TargetASC)
        {
            continue;
        }

        if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            const FVector Direction = (TargetActor->GetActorLocation() - CachedCtx.AvatarActor->GetActorLocation()).GetSafeNormal();
            FDamageEffectParams Params;
            Params.SourceAbilitySystemComponent = CachedCtx.ASC;
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
            Params.KnockbackForce = FVector::UpVector * Definition->KnockbackForceMagnitude;
            Params.KnockbackChance = Definition->KnockbackChance;
            Params.bIsRadialDamage = false;
            Params.RadialDamageInnerRadius = 0.f;
            Params.RadialDamageOuterRadius = 0.f;
            Params.RadialDamageOrigin = FVector::ZeroVector;
            UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
        }
    }

    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[ElectrocuteBeam] TickDamage applied to %d targets"), BeamTargets.Num());
}

void UElectrocuteBeamTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] OnExit status=%d"), (int32)Status);

    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
    }

    BeamTargets.Empty();
}