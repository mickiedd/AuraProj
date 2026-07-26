// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbilityGraphTypes.h"
#include "AbilityActionNode.generated.h"

class UAuraAbilityActionTask;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class AURAABILITYGRAPH_API UAuraAbilityActionNode : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityGraph|Node")
    int32 NodeId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityGraph|Node")
    FString NodeClassName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "AbilityGraph|Node")
    TArray<UAuraAbilityActionNode*> Children;

    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) {}
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const { return nullptr; }

    void AddChild(UAuraAbilityActionNode* Child)
    {
        if (Child)
        {
            Children.Add(Child);
        }
    }
};
