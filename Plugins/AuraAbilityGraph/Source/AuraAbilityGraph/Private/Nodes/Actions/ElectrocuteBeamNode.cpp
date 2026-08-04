// Copyright Druid Mechanics

#include "Nodes/Actions/ElectrocuteBeamNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "Aura/Aura.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityTypes.h"
#include "Interaction/CombatInterface.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace ElectrocuteBeamPrivate
{
    static FVector GetSocketLocation(AActor* Avatar, const FGameplayTag& SocketTag)
    {
        if (!Avatar || !Avatar->Implements<UCombatInterface>())
        {
            return FVector::ZeroVector;
        }

        // Native-only test avatars and native C++ characters can have a multiple-
        // inheritance interface layout. Calling Execute_ on those objects can land
        // in the interface base implementation instead of the derived override. Use
        // the native interface directly when available; Blueprint-only implementations
        // still go through the generated Execute_ dispatch.
        if (ICombatInterface* NativeCombat = Cast<ICombatInterface>(Avatar))
        {
            return NativeCombat->GetCombatSocketLocation_Implementation(SocketTag);
        }

        return ICombatInterface::Execute_GetCombatSocketLocation(Avatar, SocketTag);
    }

    static float GetBeamRange(const UElectrocuteBeamNode* Node)
    {
        return Node && FMath::IsFinite(Node->MaxBeamRange) ? FMath::Max(1.f, Node->MaxBeamRange) : 3000.f;
    }

    static FVector ClampEndpointToRange(const FVector& Start, const FVector& Candidate, const FVector& FallbackDirection, float MaxRange)
    {
        FVector Direction = Candidate - Start;
        float CandidateDistance = Direction.Size();
        if (Direction.ContainsNaN() || !FMath::IsFinite(CandidateDistance) || Direction.IsNearlyZero())
        {
            Direction = FallbackDirection;
            CandidateDistance = MaxRange;
        }

        Direction = Direction.GetSafeNormal();
        if (Direction.IsNearlyZero())
        {
            Direction = FVector::ForwardVector;
        }

        return Start + Direction * FMath::Min(MaxRange, FMath::Max(1.f, CandidateDistance));
    }

    static void SetBeamPosition(UNiagaraComponent* Beam, const FString& ParameterName, const FVector& Position)
    {
        if (!Beam || ParameterName.IsEmpty())
        {
            return;
        }

        // The shipped Niagara system exposes the parameters as "User.Beam Start"
        // and "User.Beam End" (the spaces are part of the parameter names). Keep
        // the authored name working as well, because older graph definitions used
        // BeamStart/BeamEnd without the Niagara User namespace.
        Beam->SetVariablePosition(FName(*ParameterName), Position);

        FString CompactName = ParameterName;
        CompactName.RemoveFromStart(TEXT("User."));
        CompactName.ReplaceInline(TEXT(" "), TEXT(""));
        if (CompactName.Equals(TEXT("BeamStart"), ESearchCase::IgnoreCase))
        {
            Beam->SetVariablePosition(FName(TEXT("User.Beam Start")), Position);
            Beam->SetVariablePosition(FName(TEXT("BeamStart")), Position);
        }
        else if (CompactName.Equals(TEXT("BeamEnd"), ESearchCase::IgnoreCase))
        {
            Beam->SetVariablePosition(FName(TEXT("User.Beam End")), Position);
            Beam->SetVariablePosition(FName(TEXT("BeamEnd")), Position);
        }
    }

}

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
        else if (Property.Name == TEXT("MaxBeamRange"))
        {
            MaxBeamRange = FCString::Atof(*Property.Value);
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

    // The target-data hit is only the activation-time snapshot. Refresh immediately
    // from the current cursor too, then keep doing so for the entire channel.
    RefreshBeamFX(CachedCtx, Node);

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

    // Cursor movement is a visual concern and must be refreshed on the casting client,
    // where the local PlayerController owns the real cursor. The server also refreshes
    // when it has a local controller (standalone/listen-server), while dedicated-server
    // instances safely retain the activation snapshot for damage authority.
    {
        FTimerDelegate RefreshDel;
        RefreshDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UElectrocuteBeamTask>(this)]()
        {
            UElectrocuteBeamTask* Task = ThisObj.Get();
            if (!Task || !Task->OwnerAbility)
            {
                return;
            }

            if (const UElectrocuteBeamNode* TaskNode = Cast<UElectrocuteBeamNode>(Task->NodeDef))
            {
                Task->RefreshBeamFX(Task->CachedCtx, TaskNode);
            }
        });
        World->GetTimerManager().SetTimer(
            BeamRefreshTimerHandle,
            RefreshDel,
            FMath::Max(0.01f, Node->TickInterval),
            true);
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

    // Get socket location. All endpoint calculations use this same launch point.
    const FVector SocketLocation = ElectrocuteBeamPrivate::GetSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    const FVector BeamDirection = Ctx.AvatarActor->GetActorForwardVector().GetSafeNormal();
    const float MaxRange = ElectrocuteBeamPrivate::GetBeamRange(Node);

    AActor* PrimaryTarget = nullptr;
    bool bPrimaryIsEnemy = false;
    bool bPrimaryVisualOnly = false;
    bool bHasPrimaryEndpoint = false;
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
            const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                ? Ctx.CursorHit.Location
                : Ctx.CursorHit.ImpactPoint;
            const FVector ClampedCursorPoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                SocketLocation, CursorPoint, BeamDirection, MaxRange);
            UAbilitySystemComponent* HitASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);

            // Landscape and static meshes are not valid Electrocute targets. Try the
            // same weapon-to-cursor path once to catch an enemy in front of the hit
            // surface; never fall through to an unrelated forward target.
            if (HitASC || HitActor->Implements<UCombatInterface>())
            {
                PrimaryTarget = HitActor;
                bPrimaryIsEnemy = HitASC != nullptr;
                const FVector CursorImpactPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                    ? HitActor->GetActorLocation()
                    : FVector(Ctx.CursorHit.ImpactPoint);
                PrimaryImpactPoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                    SocketLocation,
                    CursorImpactPoint,
                    BeamDirection,
                    MaxRange);
                bHasPrimaryEndpoint = true;
            }
            else
            {
                TArray<AActor*> TraceIgnore;
                TraceIgnore.Add(Ctx.AvatarActor);
                FHitResult TargetTrace;
                UKismetSystemLibrary::SphereTraceSingle(
                    Ctx.AvatarActor,
                    SocketLocation,
                    ClampedCursorPoint,
                    Node->TraceRadius,
                    UEngineTypes::ConvertToTraceType(ECC_Visibility),
                    false,
                    TraceIgnore,
                    EDrawDebugTrace::None,
                    TargetTrace,
                    true);

                AActor* TracedActor = TargetTrace.GetActor();
                if (TracedActor && TracedActor != Ctx.AvatarActor && TracedActor->Implements<UCombatInterface>())
                {
                    PrimaryTarget = TracedActor;
                    bPrimaryIsEnemy = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TracedActor) != nullptr;
                    PrimaryImpactPoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                        SocketLocation,
                        TargetTrace.ImpactPoint,
                        BeamDirection,
                        MaxRange);
                    bHasPrimaryEndpoint = true;
                }
            }

            if (PrimaryTarget)
            {
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Primary target (cursor/trace): %s (enemy=%s) socket=%s endpoint=%s distance=%.1f"),
                    *PrimaryTarget->GetName(),
                    bPrimaryIsEnemy ? TEXT("true") : TEXT("false"),
                    *SocketLocation.ToString(),
                    *PrimaryImpactPoint.ToString(),
                    FVector::Dist(SocketLocation, PrimaryImpactPoint));
            }
            else
            {
                // Keep a world hit as a visual-only endpoint. It must not become a
                // damage target or a chain origin, but the beam should still render
                // toward the point under the cursor as the legacy ability did.
                bPrimaryVisualOnly = true;
                PrimaryImpactPoint = ClampedCursorPoint;
                bHasPrimaryEndpoint = true;
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Cursor endpoint is visual-only actor='%s' socket=%s point=%s"),
                    *GetNameSafe(HitActor),
                    *SocketLocation.ToString(),
                    *PrimaryImpactPoint.ToString());
            }
        }
        else
        {
            // TargetDataUnderMouse can provide a deterministic point without an
            // actor when the cursor is over open sky/headless test input.
            const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                ? FVector(Ctx.CursorHit.Location)
                : FVector(Ctx.CursorHit.ImpactPoint);
            PrimaryImpactPoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                SocketLocation,
                CursorPoint,
                BeamDirection,
                MaxRange);
            bPrimaryVisualOnly = true;
            bHasPrimaryEndpoint = true;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Cursor endpoint is visual-only actor='<none>' socket=%s point=%s"),
                *SocketLocation.ToString(),
                *PrimaryImpactPoint.ToString());
        }
    }

    // Fallback: sphere trace along the pawn's facing direction when there's no cursor
    // hit (best-effort visual).
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Ctx.AvatarActor);
    if (!bHasPrimaryEndpoint)
    {
        const FVector TargetLocation = SocketLocation + BeamDirection * MaxRange;

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
            AActor* TracedActor = HitResult.GetActor();
            if (TracedActor->Implements<UCombatInterface>())
            {
                PrimaryTarget = TracedActor;
                PrimaryImpactPoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                    SocketLocation,
                    HitResult.ImpactPoint,
                    BeamDirection,
                    MaxRange);
                bPrimaryIsEnemy = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PrimaryTarget) != nullptr;
                bHasPrimaryEndpoint = true;
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Primary target (forward trace): %s (enemy=%s) socket=%s endpoint=%s distance=%.1f"),
                    *PrimaryTarget->GetName(),
                    bPrimaryIsEnemy ? TEXT("true") : TEXT("false"),
                    *SocketLocation.ToString(),
                    *PrimaryImpactPoint.ToString(),
                FVector::Dist(SocketLocation, PrimaryImpactPoint));
            }
        }

        if (!bHasPrimaryEndpoint)
        {
            PrimaryImpactPoint = TargetLocation;
            bPrimaryVisualOnly = true;
            bHasPrimaryEndpoint = true;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Forward endpoint is visual-only point=%s"),
                *PrimaryImpactPoint.ToString());
        }
    }

    if (!bHasPrimaryEndpoint)
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
    PrimaryEntry.bVisualOnly = bPrimaryVisualOnly || !bPrimaryIsEnemy;
    PrimaryEntry.bDamageTarget = bPrimaryIsEnemy;
    PrimaryEntry.bCursorDriven = true;

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
        Entry.bDamageTarget = true;
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

    const FVector SocketLocation = ElectrocuteBeamPrivate::GetSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    int32 SpawnedBeamCount = 0;
    for (FAuraBeamTarget& Entry : BeamTargets)
    {
        AActor* TargetActor = Entry.Actor.Get();
        if (!TargetActor && !Entry.bVisualOnly)
        {
            continue;
        }

        // NS_ElectricBeam's beam emitter uses absolute world-space start/end positions.
        // Keep the component at world origin so a local-space emitter (the asset has
        // shipped in that mode in some cooked versions) cannot apply the muzzle location
        // a second time to the positions we pass below. The component is only a Niagara
        // host; the actual endpoints are the User.Beam Start/End values.
        UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            BeamSystem,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            FVector::OneVector,
            /*bAutoDestroy=*/true,
            /*bAutoActivate=*/false,
            ENCPoolMethod::None,
            /*bAutoDestroyWhenDeactivated=*/true);

        if (!Beam)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[ElectrocuteBeam] Failed to spawn beam component for target %s"), *GetNameSafe(TargetActor));
            continue;
        }

        ElectrocuteBeamPrivate::SetBeamPosition(Beam, Node->BeamStartParameter, SocketLocation);
        ElectrocuteBeamPrivate::SetBeamPosition(Beam, Node->BeamEndParameter, Entry.BeamEndLocation);
        Beam->Activate(true);
        Entry.Beam = Beam;
        ++SpawnedBeamCount;

        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Beam endpoints start=%s end=%s component=%s target=%s"),
            *SocketLocation.ToString(),
            *Entry.BeamEndLocation.ToString(),
            *Beam->GetComponentLocation().ToString(),
            *GetNameSafe(TargetActor));
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[ElectrocuteBeam] Spawned %d beam arc(s) from '%s'"), SpawnedBeamCount, *Node->BeamEffect);
}

void UElectrocuteBeamTask::RefreshBeamFX(const FAuraAbilityExecutionContext& Ctx, const UElectrocuteBeamNode* Node)
{
    if (Node->BeamEffect.IsEmpty() || !Ctx.AvatarActor)
    {
        return;
    }

    const FVector SocketLocation = ElectrocuteBeamPrivate::GetSocketLocation(Ctx.AvatarActor, Node->SocketTag);

    for (FAuraBeamTarget& Entry : BeamTargets)
    {
        if (Entry.bCursorDriven)
        {
            FVector LiveCursorEndpoint;
            if (ResolveLiveCursorEndpoint(Ctx, Node, LiveCursorEndpoint))
            {
                Entry.BeamEndLocation = LiveCursorEndpoint;
            }
        }
        // Chain arcs are target-driven and track the chained actor. The primary arc is
        // deliberately cursor-driven even when its initial cursor hit was an enemy.
        else if (Entry.bDamageTarget)
        {
            if (AActor* TargetActor = Entry.Actor.Get())
            {
                if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
                {
                    Entry.BeamEndLocation = TargetActor->GetActorLocation();
                }
            }
        }

        UNiagaraComponent* Beam = Entry.Beam.Get();
        if (!Beam)
        {
            continue;
        }
        ElectrocuteBeamPrivate::SetBeamPosition(Beam, Node->BeamStartParameter, SocketLocation);
        ElectrocuteBeamPrivate::SetBeamPosition(Beam, Node->BeamEndParameter, Entry.BeamEndLocation);
    }
}

bool UElectrocuteBeamTask::ResolveLiveCursorEndpoint(
    const FAuraAbilityExecutionContext& Ctx,
    const UElectrocuteBeamNode* Node,
    FVector& OutEndpoint) const
{
    if (!Ctx.AvatarActor || !Node)
    {
        return false;
    }

    const FVector SocketLocation = ElectrocuteBeamPrivate::GetSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    const FVector BeamDirection = Ctx.AvatarActor->GetActorForwardVector().GetSafeNormal();
    const float MaxRange = ElectrocuteBeamPrivate::GetBeamRange(Node);

    // Only a local controller can query the live mouse. On a dedicated server use the
    // activation snapshot; the casting client owns the visual cursor update.
    APlayerController* LocalPC = nullptr;
    if (OwnerAbility)
    {
        if (const FGameplayAbilityActorInfo* ActorInfo = OwnerAbility->GetCurrentActorInfo())
        {
            LocalPC = ActorInfo->PlayerController.Get();
        }
    }

    if (LocalPC && LocalPC->IsLocalController())
    {
        FHitResult LiveCursorHit;
        LocalPC->GetHitResultUnderCursor(ECC_Target, false, LiveCursorHit);
        if (LiveCursorHit.bBlockingHit)
        {
            const FVector CursorPoint = LiveCursorHit.ImpactPoint.IsNearlyZero()
                ? LiveCursorHit.Location
                : LiveCursorHit.ImpactPoint;
            OutEndpoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
                SocketLocation,
                CursorPoint,
                BeamDirection,
                MaxRange);
            return true;
        }

        // Match TargetDataUnderMouse's deterministic open-sky fallback rather than
        // retaining a stale activation point when the cursor leaves world geometry.
        OutEndpoint = SocketLocation + BeamDirection * MaxRange;
        return true;
    }

    if (Ctx.CursorHit.bBlockingHit)
    {
        const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
            ? Ctx.CursorHit.Location
            : Ctx.CursorHit.ImpactPoint;
        OutEndpoint = ElectrocuteBeamPrivate::ClampEndpointToRange(
            SocketLocation,
            CursorPoint,
            BeamDirection,
            MaxRange);
        return true;
    }

    OutEndpoint = SocketLocation + BeamDirection * MaxRange;
    return true;
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
        if (!BeamTargets[i].Actor.IsValid() && !BeamTargets[i].bVisualOnly)
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
    for (const FAuraBeamTarget& Entry : BeamTargets)
    {
        AActor* TargetActor = Entry.Actor.Get();
        if (!TargetActor)
        {
            continue;
        }

        if (!Entry.bDamageTarget)
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
            Definition->BuildDamageEffectParams(Params, CachedCtx.ASC, TargetASC, CachedCtx.AvatarActor, DataAbility->GetAbilityLevel(), Direction);
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
        World->GetTimerManager().ClearTimer(BeamRefreshTimerHandle);
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
        World->GetTimerManager().ClearTimer(BeamRefreshTimerHandle);
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

FVector UElectrocuteBeamTask::GetBeamStartLocationForTest() const
{
    const UElectrocuteBeamNode* Node = Cast<UElectrocuteBeamNode>(NodeDef);
    if (Node && CachedCtx.AvatarActor && CachedCtx.AvatarActor->Implements<UCombatInterface>())
    {
        return ElectrocuteBeamPrivate::GetSocketLocation(CachedCtx.AvatarActor, Node->SocketTag);
    }
    return FVector::ZeroVector;
}

FVector UElectrocuteBeamTask::GetBeamComponentLocationForTest(int32 Index) const
{
    if (BeamTargets.IsValidIndex(Index))
    {
        if (const UNiagaraComponent* Beam = BeamTargets[Index].Beam.Get())
        {
            return Beam->GetComponentLocation();
        }
    }
    return FVector::ZeroVector;
}

void UElectrocuteBeamTask::UpdateCursorForTest(const FHitResult& CursorHit)
{
    CachedCtx.CursorHit = CursorHit;
    if (const UElectrocuteBeamNode* Node = Cast<UElectrocuteBeamNode>(NodeDef))
    {
        RefreshBeamFX(CachedCtx, Node);
    }
}
