// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "ModularBeamNodes.generated.h"

/** Resolves and stores the combat socket used as the beam origin. */
UCLASS(DisplayName = "ResolveBeamOrigin")
class AURAABILITYGRAPH_API UResolveBeamOriginNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FGameplayTag SocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    float MaxRange = 3000.f;
};

UCLASS()
class AURAABILITYGRAPH_API UResolveBeamOriginTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Acquires the cursor target and creates the primary visual/damage entry. */
UCLASS(DisplayName = "AcquirePrimaryBeamTarget")
class AURAABILITYGRAPH_API UAcquirePrimaryBeamTargetNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    float TraceRadius = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    float MaxRange = 3000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    bool bAllowVisualOnlyEndpoint = true;
};

UCLASS()
class AURAABILITYGRAPH_API UAcquirePrimaryBeamTargetTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Selects the fixed set of additional targets used for the channel. */
UCLASS(DisplayName = "SelectBeamChainTargets")
class AURAABILITYGRAPH_API USelectBeamChainTargetsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    float SearchRadius = 850.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    int32 MaxAdditionalTargets = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    bool bReplaceInvalidTargets = false;
};

UCLASS()
class AURAABILITYGRAPH_API USelectBeamChainTargetsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Spawns one Niagara component per beam target. */
UCLASS(DisplayName = "SpawnBeamVisuals")
class AURAABILITYGRAPH_API USpawnBeamVisualsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamEffect = TEXT("/Game/Assets/Effects/Shock/NS_ElectricBeam.NS_ElectricBeam");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamStartParameter = TEXT("User.Beam Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamEndParameter = TEXT("User.Beam End");
};

UCLASS()
class AURAABILITYGRAPH_API USpawnBeamVisualsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Writes the initial world-space endpoints to the spawned Niagara components. */
UCLASS(DisplayName = "InitializeBeamEndpoints")
class AURAABILITYGRAPH_API UInitializeBeamEndpointsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamStartParameter = TEXT("User.Beam Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamEndParameter = TEXT("User.Beam End");
};

UCLASS()
class AURAABILITYGRAPH_API UInitializeBeamEndpointsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Generic timed child-loop used to orchestrate the channel tick. */
UCLASS(DisplayName = "TimedLoop")
class AURAABILITYGRAPH_API UTimedLoopNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedLoop")
    float Duration = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedLoop")
    float Interval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedLoop")
    bool bExecuteImmediately = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedLoop")
    bool bCleanupBeamStateOnCancel = true;
};

UCLASS()
class AURAABILITYGRAPH_API UTimedLoopTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;

private:
    FTimerHandle TickTimerHandle;
    FTimerHandle DurationTimerHandle;
    FAuraAbilityExecutionContext CachedCtx;

    EAuraAbilityActionStatus RunIteration(FAuraAbilityExecutionContext& Ctx);
    void CleanupBeamState(FAuraAbilityExecutionContext& Ctx);
};

/** Removes dead actor entries and their associated visuals. */
UCLASS(DisplayName = "PruneBeamTargets")
class AURAABILITYGRAPH_API UPruneBeamTargetsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAABILITYGRAPH_API UPruneBeamTargetsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Refreshes the cursor-driven primary endpoint and moving chain endpoints. */
UCLASS(DisplayName = "RefreshBeamEndpoints")
class AURAABILITYGRAPH_API URefreshBeamEndpointsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamStartParameter = TEXT("User.Beam Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    FString BeamEndParameter = TEXT("User.Beam End");
};

UCLASS()
class AURAABILITYGRAPH_API URefreshBeamEndpointsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Applies the definition damage to every damageable entry in the shared target set. */
UCLASS(DisplayName = "ApplyBeamDamage")
class AURAABILITYGRAPH_API UApplyBeamDamageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam")
    bool bAuthorityOnly = true;
};

UCLASS()
class AURAABILITYGRAPH_API UApplyBeamDamageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};

/** Destroys beam components and clears the shared beam state. */
UCLASS(DisplayName = "DestroyBeamVisuals")
class AURAABILITYGRAPH_API UDestroyBeamVisualsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAABILITYGRAPH_API UDestroyBeamVisualsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
