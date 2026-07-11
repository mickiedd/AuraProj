// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviorUTypes.h"
#include "BehaviorUBehaviorNode.generated.h"

class UBehaviorUBehaviorTask;
class UBehaviorUAgentComponent;
class UBehaviorUAttachment;

/**
 * Base class for all behavior tree nodes.
 * 
 * This mirrors the original behavioru BehaviorNode class, restructured
 * as a UObject for UE5 integration. Each node is a template that defines
 * behavior; the actual runtime state is held in UBehaviorUBehaviorTask.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIORURUNTIME_API UBehaviorUBehaviorNode : public UObject
{
	GENERATED_BODY()

public:
	UBehaviorUBehaviorNode();

	/** Unique node ID within the tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Node")
	int32 NodeId;

	/** Human-readable class name for serialization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Node")
	FString NodeClassName;

	/** User-specified agent type for this node */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Node")
	FString AgentType;

	/** Child nodes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|Node")
	TArray<UBehaviorUBehaviorNode*> Children;

	/** Custom node identifier (user-assigned) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Node")
	FString CustomCondition;

	/** Whether this node has custom preconditions/effectors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Node")
	bool bHasEvents;

	// --- Attachments ---

	/** Precondition attachments evaluated before this node executes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|Attachments")
	TArray<UBehaviorUAttachment*> Preconditions;

	/** Effector attachments applied after this node completes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|Attachments")
	TArray<UBehaviorUAttachment*> Effectors;

	/** Event attachments */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|Attachments")
	TArray<UBehaviorUAttachment*> Events;

	// --- Node Lifecycle ---

	/** Load properties from deserialized data */
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties);

	/** Add a child node */
	void AddChild(UBehaviorUBehaviorNode* Child);

	/** Create the corresponding task instance for this node */
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const;

	/** Check if this node is valid for the given agent */
	virtual bool IsValid(UBehaviorUAgentComponent* Agent, UBehaviorUBehaviorTask* Task) const;

	/** Get the number of children */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|Node")
	int32 GetChildCount() const { return Children.Num(); }

	/** Get child at index */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|Node")
	UBehaviorUBehaviorNode* GetChild(int32 Index) const;

	/** Get parent node (if any) */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|Node")
	UBehaviorUBehaviorNode* GetParent() const { return ParentNode; }

	/** Set parent node */
	void SetParent(UBehaviorUBehaviorNode* InParent) { ParentNode = InParent; }

protected:
	/** Parent node reference */
	UPROPERTY()
	UBehaviorUBehaviorNode* ParentNode;
};
