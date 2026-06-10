// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviacTypes.h"
#include "BehaviacBehaviorNode.generated.h"

class UBehaviacBehaviorTask;
class UBehaviacAgentComponent;
class UBehaviacAttachment;

/**
 * Base class for all behavior tree nodes.
 * 
 * This mirrors the original behaviac BehaviorNode class, restructured
 * as a UObject for UE5 integration. Each node is a template that defines
 * behavior; the actual runtime state is held in UBehaviacBehaviorTask.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIACRUNTIME_API UBehaviacBehaviorNode : public UObject
{
	GENERATED_BODY()

public:
	UBehaviacBehaviorNode();

	/** Unique node ID within the tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Node")
	int32 NodeId;

	/** Human-readable class name for serialization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Node")
	FString NodeClassName;

	/** User-specified agent type for this node */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Node")
	FString AgentType;

	/** Child nodes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|Node")
	TArray<UBehaviacBehaviorNode*> Children;

	/** Custom node identifier (user-assigned) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Node")
	FString CustomCondition;

	/** Whether this node has custom preconditions/effectors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Node")
	bool bHasEvents;

	// --- Attachments ---

	/** Precondition attachments evaluated before this node executes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|Attachments")
	TArray<UBehaviacAttachment*> Preconditions;

	/** Effector attachments applied after this node completes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|Attachments")
	TArray<UBehaviacAttachment*> Effectors;

	/** Event attachments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|Attachments")
	TArray<UBehaviacAttachment*> Events;

	// --- Node Lifecycle ---

	/** Load properties from deserialized data */
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties);

	/** Add a child node */
	void AddChild(UBehaviacBehaviorNode* Child);

	/** Create the corresponding task instance for this node */
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const;

	/** Check if this node is valid for the given agent */
	virtual bool IsValid(UBehaviacAgentComponent* Agent, UBehaviacBehaviorTask* Task) const;

	/** Get the number of children */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Node")
	int32 GetChildCount() const { return Children.Num(); }

	/** Get child at index */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Node")
	UBehaviacBehaviorNode* GetChild(int32 Index) const;

	/** Get parent node (if any) */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Node")
	UBehaviacBehaviorNode* GetParent() const { return ParentNode; }

	/** Set parent node */
	void SetParent(UBehaviacBehaviorNode* InParent) { ParentNode = InParent; }

protected:
	/** Parent node reference */
	UPROPERTY()
	UBehaviacBehaviorNode* ParentNode;
};
