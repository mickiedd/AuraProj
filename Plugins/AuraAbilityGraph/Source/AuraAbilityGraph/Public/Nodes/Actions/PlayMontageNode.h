// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "Animation/AnimMontage.h"
#include "PlayMontageNode.generated.h"

UCLASS(DisplayName = "PlayMontage")
class AURAABILITYGRAPH_API UPlayMontageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    // Asset path of the montage to play (mirrors SpawnProjectiles::ProjectileClass string
    // convention). Loaded once into `Montage` at parse time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayMontage")
    FString MontagePath;

    // Montage loaded from MontagePath. Cached on the node (which lives in the GC-rooted
    // RootNode tree) so it stays resident for the ability's whole lifetime — a transient
    // load only on the task could be GC'd mid-cast.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayMontage")
    TObjectPtr<UAnimMontage> Montage;
};

UCLASS()
class AURAABILITYGRAPH_API UPlayMontageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
};
