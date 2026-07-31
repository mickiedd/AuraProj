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
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

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
        else if (Property.Name == TEXT("ChannelDuration"))
        {
            ChannelDuration = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("GameplayCueTag"))
        {
            // Legacy field kept for backward compatibility with older XML; the beam
            // visual is now driven by BeamEffect + BeamStart/EndParameter below.
        }
        else if (Property.Name == TEXT("BeamEffect"))
        {
            BeamEffect = Property.Value;
        }
        else if (Property.Name == TEXT("BeamStartParameter"))
        {
            BeamStartParameter = Property.Value;
        }
        else if (Property.Name == TEXT("BeamEndParameter"))
        {
            BeamEndParameter = Property.Value;
        }
    }

    // Default the socket tag if the XML didn't set one. Without this, an empty SocketTag
    // makes GetCombatSocketLocation return world origin, so the beam's sphere trace runs
    // from origin along the pawn's forward and finds no (or wrong) targets — the
    // "No primary target found from trace" / beam-not-rendering symptom. Requested at
    // load time (well after native gameplay tags are registered), not in the CDO ctor.
    if (!SocketTag.IsValid())
    {
        SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
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

    const UElectrocuteBeamNode* Node = Cast<UElectrocuteBeamNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] OnStart abort: NodeDef is not UElectrocuteBeamNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    CachedCtx = Ctx;

    // Resolve beam targets on every machine. The server uses them for damage;
    // the casting client uses them to draw the arc. Remote clients re-trace with
    // their own cursor hit (best-effort visual).
    FindBeamTargets(Ctx, Node);

    if (BeamTargets.Num() == 0)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] OnStart no targets found, completing"));
        return EAuraAbilityActionStatus::Success;
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] OnStart found %d targets%s"),
        BeamTargets.Num(), Ctx.AvatarActor->HasAuthority() ? TEXT(" (authority, starting tick timer)") : TEXT(" (non-authority, visual only)"));

    // Spawn the beam arc on every machine so the casting player sees the lightning.
    SpawnBeamFX(Ctx, Node);

    UWorld* World = Ctx.AvatarActor->GetWorld();
    if (!World)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    // Channel-end timer (every machine): after ChannelDuration the beam completes on
    // its own so the ability ends and can be re-triggered. Without a finite duration
    // the beam returns Running forever and the ability never ends (second press of the
    // input is ignored because GAS won't reactivate an still-active ability). The beam
    // also ends early if all targets die first (see TickDamage). Set PendingStatus so
    // the next AdvanceGraph resumes the Sequence past this node instead of re-running
    // OnStart (which would duplicate the beams/timers).
    if (Node->ChannelDuration > 0.f)
    {
        FTimerDelegate EndDel;
        // Weak captures (ability + task) so a PIE teardown mid-channel can't root the
        // task/World via the timer delegate — same rationale as the tick timer below.
        EndDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UElectrocuteBeamTask>(this)]()
        {
            UElectrocuteBeamTask* Task = ThisObj.Get();
            if (!Task || !Task->OwnerAbility)
            {
                return;
            }
            Task->PendingStatus = EAuraAbilityActionStatus::Success;
            if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(Task->OwnerAbility))
            {
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] ChannelDuration elapsed, ending beam"));
                DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
            }
        });
        World->GetTimerManager().SetTimer(ChannelTimerHandle, EndDel, Node->ChannelDuration, false);
    }

    // Non-authority machines stop here: visual only, no damage. Stay Running so the
    // arc persists until the ability ends (OnExit/Cancel tears it down).
    if (!Ctx.AvatarActor->HasAuthority())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] OnStart skipped on non-authority (visual only)"));
        return EAuraAbilityActionStatus::Running;
    }

    // Authority: do first damage tick immediately, then set up the repeating timer.
    TickDamage();

    FTimerDelegate TimerDel;
    // Capture the task WEAKLY, not strongly: a TStrongObjectPtr here would root the task for the
    // lifetime of the timer delegate. If the PIE world tears down while the beam is still
    // channeling (OnExit never runs, so the timer is never cleared), that strong capture keeps
    // the task — and its Outer chain up to the World — root-set, so the old PIE World can't be
    // GC'd and the editor's EndPlayMap stale-reference ensure fires. BindWeakLambda already gates
    // execution on OwnerAbility validity; the weak task capture handles the task-being-GC'd case
    // without rooting it.
    TimerDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UElectrocuteBeamTask>(this)]()
    {
        UElectrocuteBeamTask* Task = ThisObj.Get();
        if (!Task || !Task->OwnerAbility)
        {
            return;
        }
        Task->TickDamage();
    });

    World->GetTimerManager().SetTimer(TickTimerHandle, TimerDel, Node->TickInterval, true);

    // Return Running — the beam continues until ChannelDuration elapses, all targets
    // die, or the ability is cancelled.
    return EAuraAbilityActionStatus::Running;
}

void UElectrocuteBeamTask::FindBeamTargets(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node)
{
    BeamTargets.Empty();

    // Get socket location
    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    AActor* PrimaryTarget = nullptr;
    bool bPrimaryIsEnemy = false;
    FVector PrimaryImpactPoint = FVector::ZeroVector;

    // Prefer the cursor-hit actor as the primary target. The original Electrocute
    // connected the beam to the CLICKED enemy (TargetDataUnderMouse -> SpawnElectricBeam).
    // The forward-only trace the port used can't reach a distant cursor target and — with
    // SocketTag unset — started at world origin, so it repeatedly found nothing; the beam
    // then completed immediately and EndAbility cut the cast montage short (the
    // "montage never finishes" symptom).
    if (Ctx.CursorHit.bBlockingHit)
    {
        AActor* HitActor = Ctx.CursorHit.GetActor();
        if (HitActor && HitActor != Ctx.AvatarActor)
        {
            PrimaryTarget = HitActor;
            PrimaryImpactPoint = Ctx.CursorHit.ImpactPoint;
            bPrimaryIsEnemy = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PrimaryTarget) != nullptr;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Primary target (cursor): %s (enemy=%s) impact=%s"),
                *PrimaryTarget->GetName(), bPrimaryIsEnemy ? TEXT("true") : TEXT("false"), *PrimaryImpactPoint.ToString());
        }
    }

    // Fallback: sphere trace along the pawn's facing direction when there's no cursor
    // hit (best-effort visual).
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Ctx.AvatarActor);
    if (!PrimaryTarget)
    {
        const FVector BeamDirection = Ctx.AvatarActor->GetActorForwardVector();
        const FVector TargetLocation = SocketLocation + BeamDirection * 1000.f;

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

        if (HitResult.bBlockingHit && HitResult.GetActor())
        {
            PrimaryTarget = HitResult.GetActor();
            PrimaryImpactPoint = HitResult.ImpactPoint;
            bPrimaryIsEnemy = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PrimaryTarget) != nullptr;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Primary target (trace): %s (enemy=%s) impact=%s"),
                *PrimaryTarget->GetName(), bPrimaryIsEnemy ? TEXT("true") : TEXT("false"), *PrimaryImpactPoint.ToString());
        }
    }

    if (!PrimaryTarget)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] No primary target found"));
        return;
    }

    FAuraBeamTarget& PrimaryEntry = BeamTargets.AddDefaulted_GetRef();
    PrimaryEntry.Actor = PrimaryTarget;
    // Drive the beam end from the impact point, NOT the actor's pivot. For a real enemy
    // the impact point is on its body (where the mouse aimed); for a ground/wall hit the
    // impact point is at the cursor.
    PrimaryEntry.BeamEndLocation = PrimaryImpactPoint;

    // Only chain lightning off a real enemy. A ground/wall primary has no meaningful
    // origin to chain from, so chains would arc to random enemies near (0,0,0).
    if (!bPrimaryIsEnemy)
    {
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
        FAuraBeamTarget& Entry = BeamTargets.AddDefaulted_GetRef();
        Entry.Actor = Target;
        Entry.BeamEndLocation = Target->GetActorLocation();
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Chain target: %s"), *Target->GetName());
    }
}

void UElectrocuteBeamTask::SpawnBeamFX(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node)
{
    if (Node->BeamEffect.IsEmpty())
    {
        return;
    }

    UWorld* World = Ctx.AvatarActor ? Ctx.AvatarActor->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    UNiagaraSystem* BeamSystem = LoadObject<UNiagaraSystem>(nullptr, *Node->BeamEffect);
    if (!BeamSystem)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] Failed to load BeamEffect '%s'"), *Node->BeamEffect);
        return;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    for (FAuraBeamTarget& Entry : BeamTargets)
    {
        AActor* TargetActor = Entry.Actor.Get();
        if (!TargetActor)
        {
            continue;
        }

        // Spawn at the socket location (world space). The beam renderer reads
        // BeamStart/BeamEnd as absolute world positions, so attachment is not needed.
        UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            BeamSystem,
            SocketLocation,
            FRotator::ZeroRotator,
            FVector::OneVector,
            /*bAutoDestroy=*/true,
            /*bAutoActivate=*/true,
            ENCPoolMethod::None,
            /*bAutoDestroyWhenDeactivated=*/true);

        if (!Beam)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] Failed to spawn beam component for target %s"), *TargetActor->GetName());
            continue;
        }

        Beam->SetNiagaraVariablePosition(Node->BeamStartParameter, SocketLocation);
        Beam->SetNiagaraVariablePosition(Node->BeamEndParameter, Entry.BeamEndLocation);
        Entry.Beam = Beam;
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Spawned %d beam arc(s) from '%s'"), BeamTargets.Num(), *Node->BeamEffect);
}

void UElectrocuteBeamTask::RefreshBeamFX(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node)
{
    if (Node->BeamEffect.IsEmpty() || !Ctx.AvatarActor)
    {
        return;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    for (FAuraBeamTarget& Entry : BeamTargets)
    {
        UNiagaraComponent* Beam = Entry.Beam.Get();
        if (!Beam)
        {
            continue;
        }
        Beam->SetNiagaraVariablePosition(Node->BeamStartParameter, SocketLocation);
        // Track a real enemy's current location; for a non-enemy (ground/wall) keep the
        // stored impact point so the arc stays where the cursor aimed.
        if (AActor* TargetActor = Entry.Actor.Get())
        {
            if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
            {
                Entry.BeamEndLocation = TargetActor->GetActorLocation();
            }
        }
        Beam->SetNiagaraVariablePosition(Node->BeamEndParameter, Entry.BeamEndLocation);
    }
}

void UElectrocuteBeamTask::TickDamage()
{
    if (!OwnerAbility || !CachedCtx.AvatarActor)
    {
        return;
    }

    // Clean up dead/invalid targets
    const int32 Before = BeamTargets.Num();
    for (int32 i = BeamTargets.Num() - 1; i >= 0; --i)
    {
        if (!BeamTargets[i].Actor.IsValid())
        {
            if (UNiagaraComponent* Beam = BeamTargets[i].Beam.Get())
            {
                if (UWorld* BeamWorld = Beam->GetWorld(); BeamWorld && !BeamWorld->bIsTearingDown)
                {
                    Beam->DestroyInstance();
                }
            }
            BeamTargets.RemoveAt(i, EAllowShrinking::No);
        }
    }

    if (BeamTargets.Num() == 0)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] All targets dead/invalid (was %d), ending beam"), Before);
        if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            // Set PendingStatus so the Sequence advances past this node instead of
            // re-running OnStart (which would re-spawn beams/re-arm timers).
            PendingStatus = EAuraAbilityActionStatus::Success;
            DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
        }
        return;
    }

    const UAuraAbilityDefinition* Definition = CachedCtx.Definition;
    if (!Definition)
    {
        return;
    }

    const UElectrocuteBeamNode* Node = Cast<UElectrocuteBeamNode>(NodeDef);

    // Refresh the beam arcs so they track moving targets each tick (server only —
    // the client has no timer, so its arc is static at spawn).
    if (Node)
    {
        RefreshBeamFX(CachedCtx, Node);
    }

    for (const FAuraBeamTarget& Entry : BeamTargets)
    {
        AActor* TargetActor = Entry.Actor.Get();
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

void UElectrocuteBeamTask::CleanupBeams()
{
    for (FAuraBeamTarget& Entry : BeamTargets)
    {
        if (UNiagaraComponent* Beam = Entry.Beam.Get())
        {
            // During EndPlayMap the World is tearing down and the component is registered
            // with that World — calling DestroyInstance() here can touch a half-destroyed
            // World (this is exactly the teardown crash the weak-capture design tried to
            // avoid). If the World is already tearing down, just release our weak ref and
            // let world cleanup destroy the component. In normal play (not tearing down)
            // we destroy the instance so the arc disappears immediately on ability end.
            if (UWorld* BeamWorld = Beam->GetWorld(); BeamWorld && !BeamWorld->bIsTearingDown)
            {
                Beam->DestroyInstance();
            }
        }
        Entry.Beam = nullptr;
    }
}

void UElectrocuteBeamTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] OnExit status=%d"), (int32)Status);

    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
        World->GetTimerManager().ClearTimer(ChannelTimerHandle);
    }

    CleanupBeams();
    BeamTargets.Empty();
}

void UElectrocuteBeamTask::Cancel(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[ElectrocuteBeam] Cancel"));

    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
        World->GetTimerManager().ClearTimer(ChannelTimerHandle);
    }

    CleanupBeams();
    BeamTargets.Empty();
}

bool UElectrocuteBeamTask::HasBeamComponentForTest() const
{
    for (const FAuraBeamTarget& Entry : BeamTargets)
    {
        if (Entry.Beam.IsValid())
        {
            return true;
        }
    }
    return false;
}

FVector UElectrocuteBeamTask::GetBeamEndLocationForTest(int32 Index) const
{
    if (BeamTargets.IsValidIndex(Index))
    {
        return BeamTargets[Index].BeamEndLocation;
    }
    return FVector::ZeroVector;
}