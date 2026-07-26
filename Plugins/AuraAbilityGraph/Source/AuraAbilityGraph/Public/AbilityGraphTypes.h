// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "AbilityGraphTypes.generated.h"

class UAuraAbilityActionNode;
class UAuraAbilityDefinition;

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
};
