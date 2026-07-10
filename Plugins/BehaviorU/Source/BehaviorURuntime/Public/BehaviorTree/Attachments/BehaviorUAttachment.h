// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviorUTypes.h"
#include "BehaviorUAttachment.generated.h"

class UBehaviorUAgentComponent;

/**
 * Base class for node attachments (preconditions, effectors, events).
 * Attachments modify node behavior without changing the node itself.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIORURUNTIME_API UBehaviorUAttachment : public UObject
{
	GENERATED_BODY()

public:
	UBehaviorUAttachment();

	/** Load from property data */
	virtual void LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviorUProperty>& Properties);

	/** Evaluate this attachment (used for preconditions) */
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const;

	/** Apply this attachment (used for effectors) */
	virtual void Apply(UBehaviorUAgentComponent* Agent, bool bSuccess) const;

	/** Check if this attachment applies to the given precondition phase */
	virtual bool AppliesToPhase(EBehaviorUPreconditionPhase Phase) const;

	/** Precondition phase this applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Attachment")
	EBehaviorUPreconditionPhase PreconditionPhase;

	/** Effector phase this applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Attachment")
	EBehaviorUEffectorPhase EffectorPhase;

	/** Action result that triggers this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Attachment")
	EBehaviorUActionResult ActionResult;

	/** Whether to negate the condition result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Attachment")
	bool bNegate;
};

/**
 * Precondition: evaluated before node execution.
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIORURUNTIME_API UBehaviorUPrecondition : public UBehaviorUAttachment
{
	GENERATED_BODY()

public:
	UBehaviorUPrecondition();

	virtual void LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviorUProperty>& Properties) override;
	virtual bool AppliesToPhase(EBehaviorUPreconditionPhase Phase) const override;
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const override;

	/** The condition expression to evaluate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Precondition")
	FString ConditionExpression;

	/** Operator for comparison */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Precondition")
	EBehaviorUOperatorType Operator;

	/** Left operand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Precondition")
	FString LeftOperand;

	/** Right operand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Precondition")
	FString RightOperand;
};

/**
 * Effector: applied after node completion (success or failure).
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIORURUNTIME_API UBehaviorUEffector : public UBehaviorUAttachment
{
	GENERATED_BODY()

public:
	UBehaviorUEffector();

	virtual void LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviorUProperty>& Properties) override;
	virtual void Apply(UBehaviorUAgentComponent* Agent, bool bSuccess) const override;

	/** The action expression to execute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Effector")
	FString ActionExpression;

	/** Property to set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Effector")
	FString PropertyName;

	/** Value to set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Effector")
	FString PropertyValue;
};

/**
 * Event attachment: triggers behavior based on named events.
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIORURUNTIME_API UBehaviorUEventAttachment : public UBehaviorUAttachment
{
	GENERATED_BODY()

public:
	UBehaviorUEventAttachment();

	/** Name of the event to listen for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Event")
	FString EventName;

	/** Whether the event triggers once or repeatedly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Event")
	bool bTriggeredOnce;

	/** The subtree to run when event fires */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Event")
	FString ReferenceFilename;
};
