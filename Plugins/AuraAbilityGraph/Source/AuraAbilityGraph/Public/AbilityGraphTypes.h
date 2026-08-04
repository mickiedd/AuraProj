// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "AbilityGraphTypes.generated.h"

class UAuraAbilityActionNode;
class UAuraAbilityDefinition;
class AActor;
class UNiagaraComponent;

/**
 * Runtime state shared by the modular Electrocute beam nodes.
 *
 * The ability graph passes FAuraAbilityExecutionContext by value between
 * asynchronous tasks.  A shared state object lets the setup, channel, and
 * cleanup nodes operate on the same target set and Niagara components without
 * putting Electrocute-specific fields on every task.
 */
struct FAuraBeamTargetState
{
    TWeakObjectPtr<AActor> Actor;
    FVector BeamEndLocation = FVector::ZeroVector;
    bool bVisualOnly = false;
    bool bDamageTarget = false;
    bool bCursorDriven = false;
    TWeakObjectPtr<UNiagaraComponent> Beam;
};

struct FAuraBeamExecutionState
{
    FGameplayTag SocketTag;
    float MaxRange = 3000.f;
    FVector Origin = FVector::ZeroVector;
    TArray<FAuraBeamTargetState> Targets;
};

UENUM(BlueprintType)
enum class EAuraAbilityActionStatus : uint8
{
    Invalid    UMETA(DisplayName = "Invalid"),
    Success    UMETA(DisplayName = "Success"),
    Failure    UMETA(DisplayName = "Failure"),
    Running    UMETA(DisplayName = "Running")
};

USTRUCT(BlueprintType)
struct FAuraAbilityGraphProperty
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityGraph")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityGraph")
    FString Value;

    FAuraAbilityGraphProperty() {}
    FAuraAbilityGraphProperty(const FString& InName, const FString& InValue)
        : Name(InName), Value(InValue) {}
};

USTRUCT(BlueprintType)
struct FAuraAbilityExecutionContext
{
    GENERATED_BODY()

    UAbilitySystemComponent* ASC = nullptr;
    AActor* AvatarActor = nullptr;
    FGameplayAbilitySpecHandle SpecHandle;
    const UAuraAbilityDefinition* Definition = nullptr;
    UAuraAbilityActionNode* NodeDef = nullptr;

    FGameplayAbilityTargetDataHandle TargetDataHandle;
    FHitResult CursorHit;
    FGameplayTag CombatSocketTag;

    // Shared between modular beam setup/channel/cleanup tasks. This is not a
    // reflected field because it is transient per-activation execution state.
    TSharedPtr<FAuraBeamExecutionState> BeamState;

    // Set by a child of a timed loop to request a successful early completion.
    bool bStopCurrentTimedLoop = false;
};
