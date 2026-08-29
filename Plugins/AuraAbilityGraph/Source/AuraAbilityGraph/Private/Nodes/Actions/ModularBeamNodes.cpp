// Copyright Druid Mechanics

#include "Nodes/Actions/ModularBeamNodes.h"

#include "AbilityDefinition.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Aura/Aura.h"
#include "AuraAbilityTypes.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Combat/AuraCombatRules.h"
#include "DataAbility.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

namespace ModularBeamPrivate
{
    static TSharedPtr<FAuraBeamExecutionState> EnsureState(FAuraAbilityExecutionContext& Ctx)
    {
        if (!Ctx.BeamState.IsValid())
        {
            Ctx.BeamState = MakeShared<FAuraBeamExecutionState>();
        }
        return Ctx.BeamState;
    }

    static FVector GetSocketLocation(AActor* Avatar, const FGameplayTag& SocketTag)
    {
        if (!Avatar || !Avatar->Implements<UCombatInterface>())
        {
            return FVector::ZeroVector;
        }

        // Native-only test avatars and native C++ characters can have a multiple-
        // inheritance interface layout. Calling Execute_ on those objects can land
        // in the interface base implementation instead of the derived override.
        if (ICombatInterface* NativeCombat = Cast<ICombatInterface>(Avatar))
        {
            return NativeCombat->GetCombatSocketLocation_Implementation(SocketTag);
        }

        return ICombatInterface::Execute_GetCombatSocketLocation(Avatar, SocketTag);
    }

    static float GetMaxRange(const FAuraBeamExecutionState& State, float NodeRange)
    {
        const float Candidate = FMath::IsFinite(NodeRange) ? NodeRange : State.MaxRange;
        return FMath::Max(1.f, Candidate);
    }

    static FVector ClampEndpoint(const FVector& Start, const FVector& Candidate, const FVector& FallbackDirection, float MaxRange)
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

        Beam->SetVariablePosition(FName(*ParameterName), Position);

        // Preserve compatibility with older Niagara systems that used BeamStart/
        // BeamEnd without the User namespace or spaces.
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

    static void DeactivateBeam(FAuraBeamTargetState& Entry)
    {
        if (UNiagaraComponent* Beam = Entry.Beam.Get())
        {
            if (UWorld* World = Beam->GetWorld(); World && !World->bIsTearingDown)
            {
                // Let Niagara run the asset-authored completion/fade tail. DestroyInstance()
                // deactivates immediately and releases the system, which cuts the beam off
                // at the TimedLoop boundary instead of when the ray has fully disappeared.
                Beam->Deactivate();
            }
        }
        Entry.Beam = nullptr;
    }

    static void CleanupState(FAuraBeamExecutionState& State)
    {
        for (FAuraBeamTargetState& Entry : State.Targets)
        {
            DeactivateBeam(Entry);
        }
        State.Targets.Empty();
    }

    static bool ResolveLiveCursorEndpoint(
        const FAuraAbilityExecutionContext& Ctx,
        const UAuraDataAbility* OwnerAbility,
        const FAuraBeamExecutionState& State,
        float MaxRange,
        FVector& OutEndpoint)
    {
        if (!Ctx.AvatarActor)
        {
            return false;
        }

        const FVector BeamDirection = Ctx.AvatarActor->GetActorForwardVector().GetSafeNormal();
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
                OutEndpoint = ClampEndpoint(State.Origin, CursorPoint, BeamDirection, MaxRange);
                return true;
            }

            // Keep the activation-time endpoint when the cursor is temporarily
            // over no traceable world geometry. This is important on a listen
            // server and in headless tests: losing a live cursor hit must not
            // teleport a beam away from its selected target to max range.
            return false;
        }

        if (Ctx.CursorHit.bBlockingHit)
        {
            const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                ? Ctx.CursorHit.Location
                : Ctx.CursorHit.ImpactPoint;
            OutEndpoint = ClampEndpoint(State.Origin, CursorPoint, BeamDirection, MaxRange);
            return true;
        }

        // No activation hit and no live cursor hit means there is no new endpoint
        // to apply. The caller retains the endpoint acquired during setup.
        return false;
    }

    static void ApplyEndpointParameters(
        FAuraBeamExecutionState& State,
        const FString& BeamStartParameter,
        const FString& BeamEndParameter)
    {
        for (FAuraBeamTargetState& Entry : State.Targets)
        {
            if (UNiagaraComponent* Beam = Entry.Beam.Get())
            {
                SetBeamPosition(Beam, BeamStartParameter, State.Origin);
                SetBeamPosition(Beam, BeamEndParameter, Entry.BeamEndLocation);
            }
        }
    }

    static void SpawnBeamVisual(
        UWorld* World,
        FAuraBeamExecutionState& State,
        FAuraBeamTargetState& Entry,
        UAuraDataAbility* OwnerAbility)
    {
        if (!World || State.BeamEffect.IsEmpty())
        {
            return;
        }

        UNiagaraSystem* BeamSystem = LoadObject<UNiagaraSystem>(nullptr, *State.BeamEffect);
        if (!BeamSystem)
        {
            return;
        }

        UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            BeamSystem,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            FVector::OneVector,
            true,
            false,
            ENCPoolMethod::None,
            true);
        if (!Beam)
        {
            return;
        }

        SetBeamPosition(Beam, State.BeamStartParameter, State.Origin);
        SetBeamPosition(Beam, State.BeamEndParameter, Entry.BeamEndLocation);
        if (OwnerAbility)
        {
            OwnerAbility->TrackBeamVisual(Beam, State.SourceActor.Get());
        }
        Beam->Activate(true);
        Entry.Beam = Beam;
    }

    static bool IsActorDead(AActor* Actor)
    {
        if (!Actor)
        {
            return true;
        }

        // Native C++ actors with multiple inheritance can dispatch the
        // generated Execute_ wrapper to the interface stub instead of the
        // concrete implementation. Blueprint subclasses still use the
        // generated wrapper so Blueprint overrides remain respected.
        if (Actor->GetClass()->IsNative())
        {
            if (ICombatInterface* NativeCombat = Cast<ICombatInterface>(Actor))
            {
                return NativeCombat->IsDead_Implementation();
            }
        }

        return ICombatInterface::Execute_IsDead(Actor);
    }

    static bool IsDamageTargetEligible(AActor* SourceActor, AActor* TargetActor)
    {
        if (!SourceActor || !TargetActor || SourceActor == TargetActor ||
            !TargetActor->Implements<UCombatInterface>() ||
            IsActorDead(TargetActor))
        {
            return false;
        }

        FAuraCombatRuleContext RuleContext;
        RuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
        RuleContext.TrustedWorldContext = SourceActor;
        RuleContext.SourceActor = SourceActor;
        RuleContext.TargetActor = TargetActor;
        return FAuraCombatRules::CanDamage(SourceActor, TargetActor, RuleContext).bCanDamage;
    }
}

UAuraAbilityActionTask* UResolveBeamOriginNode::CreateTask(UObject* Outer) const
{
    return NewObject<UResolveBeamOriginTask>(Outer);
}

void UResolveBeamOriginNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("SocketTag"))
        {
            SocketTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("MaxRange"))
        {
            MaxRange = FCString::Atof(*Property.Value);
        }
    }

    if (!SocketTag.IsValid())
    {
        SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
    }
}

EAuraAbilityActionStatus UResolveBeamOriginTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const UResolveBeamOriginNode* Node = Cast<UResolveBeamOriginNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const TSharedPtr<FAuraBeamExecutionState> State = ModularBeamPrivate::EnsureState(Ctx);
    State->SocketTag = Node->SocketTag.IsValid()
        ? Node->SocketTag
        : FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
    State->MaxRange = FMath::Max(1.f, FMath::IsFinite(Node->MaxRange) ? Node->MaxRange : 3000.f);
    State->Origin = ModularBeamPrivate::GetSocketLocation(Ctx.AvatarActor, State->SocketTag);
    Ctx.CombatSocketTag = State->SocketTag;
    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* UAcquirePrimaryBeamTargetNode::CreateTask(UObject* Outer) const
{
    return NewObject<UAcquirePrimaryBeamTargetTask>(Outer);
}

void UAcquirePrimaryBeamTargetNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("TraceRadius"))
        {
            TraceRadius = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("MaxRange"))
        {
            MaxRange = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("AllowVisualOnlyEndpoint"))
        {
            bAllowVisualOnlyEndpoint = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus UAcquirePrimaryBeamTargetTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const UAcquirePrimaryBeamTargetNode* Node = Cast<UAcquirePrimaryBeamTargetNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const TSharedPtr<FAuraBeamExecutionState> State = ModularBeamPrivate::EnsureState(Ctx);
    State->Targets.Empty();
    State->Origin = ModularBeamPrivate::GetSocketLocation(Ctx.AvatarActor, State->SocketTag);
    const FVector BeamDirection = Ctx.AvatarActor->GetActorForwardVector().GetSafeNormal();
    const float MaxRange = ModularBeamPrivate::GetMaxRange(*State, Node->MaxRange);

    AActor* PrimaryTarget = nullptr;
    bool bPrimaryIsDamageable = false;
    bool bPrimaryVisualOnly = false;
    bool bHasPrimaryEndpoint = false;
    FVector PrimaryImpactPoint = FVector::ZeroVector;

    if (Ctx.CursorHit.bBlockingHit)
    {
        AActor* HitActor = Ctx.CursorHit.GetActor();
        if (HitActor && HitActor != Ctx.AvatarActor)
        {
            const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                ? Ctx.CursorHit.Location
                : Ctx.CursorHit.ImpactPoint;
            const FVector ClampedCursorPoint = ModularBeamPrivate::ClampEndpoint(
                State->Origin, CursorPoint, BeamDirection, MaxRange);
            UAbilitySystemComponent* HitASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);

            if (HitASC || HitActor->Implements<UCombatInterface>())
            {
                PrimaryTarget = HitActor;
                bPrimaryIsDamageable = HitASC != nullptr;
                const FVector CursorImpactPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                    ? HitActor->GetActorLocation()
                    : Ctx.CursorHit.ImpactPoint;
                PrimaryImpactPoint = ModularBeamPrivate::ClampEndpoint(
                    State->Origin, CursorImpactPoint, BeamDirection, MaxRange);
                bHasPrimaryEndpoint = true;
            }
            else
            {
                TArray<AActor*> TraceIgnore;
                TraceIgnore.Add(Ctx.AvatarActor);
                FHitResult TargetTrace;
                UKismetSystemLibrary::SphereTraceSingle(
                    Ctx.AvatarActor,
                    State->Origin,
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
                    bPrimaryIsDamageable = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TracedActor) != nullptr;
                    PrimaryImpactPoint = ModularBeamPrivate::ClampEndpoint(
                        State->Origin, TargetTrace.ImpactPoint, BeamDirection, MaxRange);
                    bHasPrimaryEndpoint = true;
                }
            }

            if (!PrimaryTarget && Node->bAllowVisualOnlyEndpoint)
            {
                bPrimaryVisualOnly = true;
                PrimaryImpactPoint = ClampedCursorPoint;
                bHasPrimaryEndpoint = true;
            }
        }
        else if (Node->bAllowVisualOnlyEndpoint)
        {
            const FVector CursorPoint = Ctx.CursorHit.ImpactPoint.IsNearlyZero()
                ? Ctx.CursorHit.Location
                : Ctx.CursorHit.ImpactPoint;
            PrimaryImpactPoint = ModularBeamPrivate::ClampEndpoint(
                State->Origin, CursorPoint, BeamDirection, MaxRange);
            bPrimaryVisualOnly = true;
            bHasPrimaryEndpoint = true;
        }
    }

    if (!bHasPrimaryEndpoint)
    {
        const FVector TargetLocation = State->Origin + BeamDirection * MaxRange;
        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(Ctx.AvatarActor);

        FHitResult HitResult;
        UKismetSystemLibrary::SphereTraceSingle(
            Ctx.AvatarActor,
            State->Origin,
            TargetLocation,
            Node->TraceRadius,
            UEngineTypes::ConvertToTraceType(ECC_Visibility),
            false,
            ActorsToIgnore,
            EDrawDebugTrace::None,
            HitResult,
            true);

        if (HitResult.bBlockingHit && HitResult.GetActor() && HitResult.GetActor()->Implements<UCombatInterface>())
        {
            PrimaryTarget = HitResult.GetActor();
            bPrimaryIsDamageable = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PrimaryTarget) != nullptr;
            PrimaryImpactPoint = ModularBeamPrivate::ClampEndpoint(
                State->Origin, HitResult.ImpactPoint, BeamDirection, MaxRange);
            bHasPrimaryEndpoint = true;
        }
        else if (Node->bAllowVisualOnlyEndpoint)
        {
            PrimaryImpactPoint = TargetLocation;
            bPrimaryVisualOnly = true;
            bHasPrimaryEndpoint = true;
        }
    }

    if (!bHasPrimaryEndpoint)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    // An ASC alone is not enough to make a target damageable. Apply the same
    // combat-policy gate used by the damage boundary so friendly, protected,
    // dead, or otherwise invalid actors remain visual-only endpoints and do
    // not open the chain-selection path.
    if (bPrimaryIsDamageable && !ModularBeamPrivate::IsDamageTargetEligible(Ctx.AvatarActor, PrimaryTarget))
    {
        bPrimaryIsDamageable = false;
    }

    FAuraBeamTargetState& PrimaryEntry = State->Targets.AddDefaulted_GetRef();
    PrimaryEntry.Actor = PrimaryTarget;
    PrimaryEntry.BeamEndLocation = PrimaryImpactPoint;
    PrimaryEntry.bVisualOnly = bPrimaryVisualOnly || !bPrimaryIsDamageable;
    PrimaryEntry.bDamageTarget = bPrimaryIsDamageable;
    PrimaryEntry.bCursorDriven = true;
    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* USelectBeamChainTargetsNode::CreateTask(UObject* Outer) const
{
    return NewObject<USelectBeamChainTargetsTask>(Outer);
}

void USelectBeamChainTargetsNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("SearchRadius"))
        {
            SearchRadius = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("MaxAdditionalTargets"))
        {
            MaxAdditionalTargets = FCString::Atoi(*Property.Value);
        }
        else if (Property.Name == TEXT("ReplaceInvalidTargets"))
        {
            bReplaceInvalidTargets = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus USelectBeamChainTargetsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor || !Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const USelectBeamChainTargetsNode* Node = Cast<USelectBeamChainTargetsNode>(NodeDef);
    if (!Node || Ctx.BeamState->Targets.Num() == 0)
    {
        return EAuraAbilityActionStatus::Success;
    }

    const FAuraBeamTargetState& PrimaryEntry = Ctx.BeamState->Targets[0];
    AActor* PrimaryTarget = PrimaryEntry.Actor.Get();
    if (!PrimaryTarget || !PrimaryEntry.bDamageTarget)
    {
        return EAuraAbilityActionStatus::Success;
    }

    const int32 NumAdditional = FMath::Min(
        FMath::Max(0, OwnerAbility->GetAbilityLevel() - 1),
        FMath::Max(0, Node->MaxAdditionalTargets));

    Ctx.BeamState->MaxAdditionalTargets = NumAdditional;
    Ctx.BeamState->SearchRadius = Node->SearchRadius;
    Ctx.BeamState->bReplaceInvalidTargets = Node->bReplaceInvalidTargets;

    if (NumAdditional <= 0)
    {
        return EAuraAbilityActionStatus::Success;
    }

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Ctx.AvatarActor);
    ActorsToIgnore.Add(PrimaryTarget);

    TArray<AActor*> OverlappingActors;
    UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
        Ctx.AvatarActor,
        OverlappingActors,
        ActorsToIgnore,
        Node->SearchRadius,
        PrimaryTarget->GetActorLocation());

    OverlappingActors.RemoveAll([&Ctx](AActor* Target)
    {
        return !ModularBeamPrivate::IsDamageTargetEligible(Ctx.AvatarActor, Target);
    });

    TArray<AActor*> ClosestTargets;
    UAuraAbilitySystemLibrary::GetClosestTargets(
        NumAdditional,
        OverlappingActors,
        ClosestTargets,
        PrimaryTarget->GetActorLocation());

    for (AActor* Target : ClosestTargets)
    {
        FAuraBeamTargetState& Entry = Ctx.BeamState->Targets.AddDefaulted_GetRef();
        Entry.Actor = Target;
        Entry.BeamEndLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;
        Entry.bDamageTarget = true;
        Entry.bCursorDriven = false;
    }

    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* USpawnBeamVisualsNode::CreateTask(UObject* Outer) const
{
    return NewObject<USpawnBeamVisualsTask>(Outer);
}

void USpawnBeamVisualsNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("BeamEffect"))
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
}

EAuraAbilityActionStatus USpawnBeamVisualsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor || !Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const USpawnBeamVisualsNode* Node = Cast<USpawnBeamVisualsNode>(NodeDef);
    if (!Node || Node->BeamEffect.IsEmpty())
    {
        return EAuraAbilityActionStatus::Success;
    }

    Ctx.BeamState->BeamEffect = Node->BeamEffect;
    Ctx.BeamState->BeamStartParameter = Node->BeamStartParameter;
    Ctx.BeamState->BeamEndParameter = Node->BeamEndParameter;

    UWorld* World = Ctx.AvatarActor->GetWorld();
    UNiagaraSystem* BeamSystem = World ? LoadObject<UNiagaraSystem>(nullptr, *Node->BeamEffect) : nullptr;
    if (!World || !BeamSystem)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnBeamVisuals] Failed to load BeamEffect '%s'"), *Node->BeamEffect);
        return EAuraAbilityActionStatus::Success;
    }

    for (FAuraBeamTargetState& Entry : Ctx.BeamState->Targets)
    {
        AActor* TargetActor = Entry.Actor.Get();
        if (!TargetActor && !Entry.bVisualOnly)
        {
            continue;
        }

        UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            BeamSystem,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            FVector::OneVector,
            true,
            false,
            ENCPoolMethod::None,
            true);
        if (!Beam)
        {
            continue;
        }

        ModularBeamPrivate::SetBeamPosition(Beam, Node->BeamStartParameter, Ctx.BeamState->Origin);
        ModularBeamPrivate::SetBeamPosition(Beam, Node->BeamEndParameter, Entry.BeamEndLocation);
        Ctx.BeamState->SourceActor = Ctx.AvatarActor;
        if (OwnerAbility)
        {
            OwnerAbility->TrackBeamVisual(Beam, Ctx.AvatarActor);
        }
        Beam->Activate(true);
        Entry.Beam = Beam;
    }

    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* UInitializeBeamEndpointsNode::CreateTask(UObject* Outer) const
{
    return NewObject<UInitializeBeamEndpointsTask>(Outer);
}

void UInitializeBeamEndpointsNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("BeamStartParameter"))
        {
            BeamStartParameter = Property.Value;
        }
        else if (Property.Name == TEXT("BeamEndParameter"))
        {
            BeamEndParameter = Property.Value;
        }
    }
}

EAuraAbilityActionStatus UInitializeBeamEndpointsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const UInitializeBeamEndpointsNode* Node = Cast<UInitializeBeamEndpointsNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    ModularBeamPrivate::ApplyEndpointParameters(
        *Ctx.BeamState,
        Node->BeamStartParameter,
        Node->BeamEndParameter);
    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* UTimedLoopNode::CreateTask(UObject* Outer) const
{
    return NewObject<UTimedLoopTask>(Outer);
}

void UTimedLoopNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("Duration"))
        {
            Duration = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("Interval"))
        {
            Interval = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("ExecuteImmediately"))
        {
            bExecuteImmediately = Property.Value.ToBool();
        }
        else if (Property.Name == TEXT("CleanupBeamStateOnCancel"))
        {
            bCleanupBeamStateOnCancel = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus UTimedLoopTask::RunIteration(FAuraAbilityExecutionContext& Ctx)
{
    Ctx.bStopCurrentTimedLoop = false;

    for (UAuraAbilityActionTask* ChildTask : ChildTasks)
    {
        if (!IsValid(ChildTask))
        {
            continue;
        }

        const EAuraAbilityActionStatus ChildStatus = ChildTask->Execute(Ctx);
        if (ChildStatus == EAuraAbilityActionStatus::Failure)
        {
            return EAuraAbilityActionStatus::Failure;
        }
        if (ChildStatus == EAuraAbilityActionStatus::Running)
        {
            return EAuraAbilityActionStatus::Running;
        }
    }

    if (Ctx.bStopCurrentTimedLoop)
    {
        Ctx.bStopCurrentTimedLoop = false;
        return EAuraAbilityActionStatus::Success;
    }

    return EAuraAbilityActionStatus::Success;
}

EAuraAbilityActionStatus UTimedLoopTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const UTimedLoopNode* Node = Cast<UTimedLoopNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    if (Node->bExecuteImmediately)
    {
        const EAuraAbilityActionStatus InitialStatus = RunIteration(Ctx);
        if (InitialStatus == EAuraAbilityActionStatus::Failure)
        {
            return InitialStatus;
        }
        if (InitialStatus == EAuraAbilityActionStatus::Running)
        {
            return InitialStatus;
        }

        // PruneBeamTargets requests a normal successful early completion by
        // returning success and leaving the target set empty.
        if (Ctx.BeamState.IsValid() && Ctx.BeamState->Targets.Num() == 0)
        {
            return EAuraAbilityActionStatus::Success;
        }
    }

    UWorld* World = OwnerAbility->GetWorld();
    if (!World)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    CachedCtx = Ctx;

    if (Node->Duration > 0.f)
    {
        FTimerDelegate DurationDelegate;
        DurationDelegate.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UTimedLoopTask>(this)]()
        {
            UTimedLoopTask* Task = ThisObj.Get();
            if (!Task || !Task->OwnerAbility)
            {
                return;
            }

            Task->PendingStatus = EAuraAbilityActionStatus::Success;
            if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(Task->OwnerAbility))
            {
                DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
            }
        });
        World->GetTimerManager().SetTimer(DurationTimerHandle, DurationDelegate, Node->Duration, false);
    }

    if (Node->Interval > 0.f)
    {
        FTimerDelegate TickDelegate;
        TickDelegate.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UTimedLoopTask>(this)]()
        {
            UTimedLoopTask* Task = ThisObj.Get();
            if (!Task || !Task->OwnerAbility)
            {
                return;
            }

            const EAuraAbilityActionStatus Status = Task->RunIteration(Task->CachedCtx);
            if (Status == EAuraAbilityActionStatus::Failure)
            {
                Task->PendingStatus = EAuraAbilityActionStatus::Failure;
                if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(Task->OwnerAbility))
                {
                    DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Failure);
                }
            }
            else if (Task->CachedCtx.BeamState.IsValid() && Task->CachedCtx.BeamState->Targets.Num() == 0)
            {
                Task->PendingStatus = EAuraAbilityActionStatus::Success;
                if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(Task->OwnerAbility))
                {
                    DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
                }
            }
        });
        World->GetTimerManager().SetTimer(
            TickTimerHandle,
            TickDelegate,
            FMath::Max(0.01f, Node->Interval),
            true);
    }

    return EAuraAbilityActionStatus::Running;
}

void UTimedLoopTask::CleanupBeamState(FAuraAbilityExecutionContext& Ctx)
{
    if (Ctx.BeamState.IsValid())
    {
        ModularBeamPrivate::CleanupState(*Ctx.BeamState);
    }
}

void UTimedLoopTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
        World->GetTimerManager().ClearTimer(DurationTimerHandle);
    }

    const UTimedLoopNode* Node = Cast<UTimedLoopNode>(NodeDef);
    if (Status == EAuraAbilityActionStatus::Failure && Node && Node->bCleanupBeamStateOnCancel)
    {
        CleanupBeamState(Ctx);
    }
}

void UTimedLoopTask::Cancel(FAuraAbilityExecutionContext& Ctx)
{
    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
        World->GetTimerManager().ClearTimer(DurationTimerHandle);
    }

    for (UAuraAbilityActionTask* ChildTask : ChildTasks)
    {
        if (IsValid(ChildTask) && ChildTask->HasEntered)
        {
            ChildTask->Cancel(Ctx);
            ChildTask->HasEntered = false;
        }
    }

    const UTimedLoopNode* Node = Cast<UTimedLoopNode>(NodeDef);
    if (Node && Node->bCleanupBeamStateOnCancel)
    {
        FAuraAbilityExecutionContext CleanupCtx = Ctx;
        if (!CleanupCtx.BeamState.IsValid())
        {
            CleanupCtx.BeamState = CachedCtx.BeamState;
        }
        CleanupBeamState(CleanupCtx);
    }
}

UAuraAbilityActionTask* UPruneBeamTargetsNode::CreateTask(UObject* Outer) const
{
    return NewObject<UPruneBeamTargetsTask>(Outer);
}

EAuraAbilityActionStatus UPruneBeamTargetsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    for (int32 Index = Ctx.BeamState->Targets.Num() - 1; Index >= 0; --Index)
    {
        FAuraBeamTargetState& Entry = Ctx.BeamState->Targets[Index];
        AActor* TargetActor = Entry.Actor.Get();
        const bool bDeadDamageTarget = Entry.bDamageTarget && TargetActor &&
            TargetActor->Implements<UCombatInterface>() &&
            ModularBeamPrivate::IsActorDead(TargetActor);
        if ((!Entry.Actor.IsValid() || bDeadDamageTarget) && !Entry.bVisualOnly)
        {
            ModularBeamPrivate::DeactivateBeam(Entry);
            Ctx.BeamState->Targets.RemoveAt(Index, EAllowShrinking::No);
        }
    }

    if (Ctx.BeamState->bReplaceInvalidTargets && Ctx.AvatarActor &&
        Ctx.BeamState->Targets.Num() > 0 && Ctx.BeamState->MaxAdditionalTargets > 0)
    {
        AActor* PrimaryTarget = Ctx.BeamState->Targets[0].Actor.Get();
        if (ModularBeamPrivate::IsDamageTargetEligible(Ctx.AvatarActor, PrimaryTarget))
        {
            int32 CurrentAdditionalTargets = 0;
            TArray<AActor*> ActorsToIgnore;
            ActorsToIgnore.Add(Ctx.AvatarActor);
            ActorsToIgnore.Add(PrimaryTarget);

            for (int32 Index = 1; Index < Ctx.BeamState->Targets.Num(); ++Index)
            {
                FAuraBeamTargetState& Entry = Ctx.BeamState->Targets[Index];
                if (AActor* ExistingTarget = Entry.Actor.Get())
                {
                    ActorsToIgnore.AddUnique(ExistingTarget);
                    if (Entry.bDamageTarget)
                    {
                        ++CurrentAdditionalTargets;
                    }
                }
            }

            const int32 MissingTargets = Ctx.BeamState->MaxAdditionalTargets - CurrentAdditionalTargets;
            if (MissingTargets > 0)
            {
                TArray<AActor*> OverlappingActors;
                UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
                    Ctx.AvatarActor,
                    OverlappingActors,
                    ActorsToIgnore,
                    Ctx.BeamState->SearchRadius,
                    PrimaryTarget->GetActorLocation());
                OverlappingActors.RemoveAll([&Ctx](AActor* Target)
                {
                    return !ModularBeamPrivate::IsDamageTargetEligible(Ctx.AvatarActor, Target);
                });

                TArray<AActor*> ReplacementTargets;
                UAuraAbilitySystemLibrary::GetClosestTargets(
                    MissingTargets,
                    OverlappingActors,
                    ReplacementTargets,
                    PrimaryTarget->GetActorLocation());
                for (AActor* ReplacementTarget : ReplacementTargets)
                {
                    if (!ReplacementTarget)
                    {
                        continue;
                    }

                    FAuraBeamTargetState& Entry = Ctx.BeamState->Targets.AddDefaulted_GetRef();
                    Entry.Actor = ReplacementTarget;
                    Entry.BeamEndLocation = ReplacementTarget->GetActorLocation();
                    Entry.bDamageTarget = true;
                    Entry.bCursorDriven = false;
                    ModularBeamPrivate::SpawnBeamVisual(
                        Ctx.AvatarActor->GetWorld(),
                        *Ctx.BeamState,
                        Entry,
                        OwnerAbility);
                }
            }
        }
    }

    if (Ctx.BeamState->Targets.Num() == 0)
    {
        Ctx.bStopCurrentTimedLoop = true;
    }

    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* URefreshBeamEndpointsNode::CreateTask(UObject* Outer) const
{
    return NewObject<URefreshBeamEndpointsTask>(Outer);
}

void URefreshBeamEndpointsNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("BeamStartParameter"))
        {
            BeamStartParameter = Property.Value;
        }
        else if (Property.Name == TEXT("BeamEndParameter"))
        {
            BeamEndParameter = Property.Value;
        }
    }
}

EAuraAbilityActionStatus URefreshBeamEndpointsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor || !Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const URefreshBeamEndpointsNode* Node = Cast<URefreshBeamEndpointsNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    Ctx.BeamState->Origin = ModularBeamPrivate::GetSocketLocation(
        Ctx.AvatarActor,
        Ctx.BeamState->SocketTag);
    const float MaxRange = FMath::Max(1.f, Ctx.BeamState->MaxRange);

    for (FAuraBeamTargetState& Entry : Ctx.BeamState->Targets)
    {
        if (Entry.bCursorDriven)
        {
            FVector LiveCursorEndpoint;
            if (ModularBeamPrivate::ResolveLiveCursorEndpoint(
                    Ctx,
                    OwnerAbility,
                    *Ctx.BeamState,
                    MaxRange,
                    LiveCursorEndpoint))
            {
                Entry.BeamEndLocation = LiveCursorEndpoint;
            }
        }
        else if (Entry.bDamageTarget)
        {
            if (AActor* TargetActor = Entry.Actor.Get())
            {
                Entry.BeamEndLocation = TargetActor->GetActorLocation();
            }
        }
    }

    ModularBeamPrivate::ApplyEndpointParameters(
        *Ctx.BeamState,
        Node->BeamStartParameter,
        Node->BeamEndParameter);
    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* UApplyBeamDamageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UApplyBeamDamageTask>(Outer);
}

void UApplyBeamDamageNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("AuthorityOnly"))
        {
            bAuthorityOnly = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus UApplyBeamDamageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (!OwnerAbility || !Ctx.AvatarActor || !Ctx.BeamState.IsValid())
    {
        return EAuraAbilityActionStatus::Failure;
    }

    const UApplyBeamDamageNode* Node = Cast<UApplyBeamDamageNode>(NodeDef);
    if (!Node)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    if (Node->bAuthorityOnly && !Ctx.AvatarActor->HasAuthority())
    {
        return EAuraAbilityActionStatus::Success;
    }

    const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility);
    const UAuraAbilityDefinition* Definition = Ctx.Definition ? Ctx.Definition : (DataAbility ? DataAbility->GetDefinition() : nullptr);
    if (!DataAbility || !Definition)
    {
        return EAuraAbilityActionStatus::Success;
    }

    for (const FAuraBeamTargetState& Entry : Ctx.BeamState->Targets)
    {
        if (!Entry.bDamageTarget)
        {
            continue;
        }

        AActor* TargetActor = Entry.Actor.Get();
        UAbilitySystemComponent* TargetASC = TargetActor
            ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor)
            : nullptr;
        if (!TargetASC)
        {
            continue;
        }

        const FVector Direction = (TargetActor->GetActorLocation() - Ctx.AvatarActor->GetActorLocation()).GetSafeNormal();
        FDamageEffectParams Params;
        Definition->BuildDamageEffectParams(
            Params,
            Ctx.ASC,
            TargetASC,
            Ctx.AvatarActor,
            DataAbility->GetAbilityLevel(),
            Direction);
        UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
    }

    return EAuraAbilityActionStatus::Success;
}

UAuraAbilityActionTask* UDestroyBeamVisualsNode::CreateTask(UObject* Outer) const
{
    return NewObject<UDestroyBeamVisualsTask>(Outer);
}

EAuraAbilityActionStatus UDestroyBeamVisualsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    if (Ctx.BeamState.IsValid())
    {
        ModularBeamPrivate::CleanupState(*Ctx.BeamState);
    }
    return EAuraAbilityActionStatus::Success;
}
