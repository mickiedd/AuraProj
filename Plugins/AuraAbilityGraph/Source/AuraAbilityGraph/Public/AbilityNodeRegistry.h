// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilityGraphTypes.h"

class UAuraAbilityActionNode;

struct FAuraAbilityNodeRegistry
{
    using FNodeFactory = TFunction<UAuraAbilityActionNode*(UObject* Outer)>;

    static FAuraAbilityNodeRegistry& Get();
    void Register(const FString& ClassName, FNodeFactory Factory);
    UAuraAbilityActionNode* Create(const FString& ClassName, UObject* Outer) const;

private:
    FAuraAbilityNodeRegistry() = default;
    mutable FRWLock Lock;
    TMap<FString, FNodeFactory> Factories;
};
