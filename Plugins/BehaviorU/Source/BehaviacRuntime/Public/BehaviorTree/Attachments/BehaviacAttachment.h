// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviacTypes.h"
#include "BehaviacAttachment.generated.h"

class UBehaviacAgentComponent;

/**
 * Base class for node attachments (preconditions, effectors, events).
 * Attachments modify node behavior without changing the node itself.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIACRUNTIME_API UBehaviacAttachment : public UObject
{
	GENERATED_BODY()

public:
	UBehaviacAttachment();

	/** Load from property data */
	virtual void LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviacProperty>& Properties);

	/** Evaluate this attachment (used for preconditions) */
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const;

	/** Apply this attachment (used for effectors) */
	virtual void Apply(UBehaviacAgentComponent* Agent, bool bSuccess) const;

	/** Check if this attachment applies to the given precondition phase */
	virtual bool AppliesToPhase(EBehaviacPreconditionPhase Phase) const;

	/** Precondition phase this applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Attachment")
	EBehaviacPreconditionPhase PreconditionPhase;

	/** Effector phase this applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Attachment")
	EBehaviacEffectorPhase EffectorPhase;

	/** Action result that triggers this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Attachment")
	EBehaviacActionResult ActionResult;

	/** Whether to negate the condition result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Attachment")
	bool bNegate;
};

/**
 * Precondition: evaluated before node execution.
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIACRUNTIME_API UBehaviacPrecondition : public UBehaviacAttachment
{
	GENERATED_BODY()

public:
	UBehaviacPrecondition();

	virtual void LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviacProperty>& Properties) override;
	virtual bool AppliesToPhase(EBehaviacPreconditionPhase Phase) const override;
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const override;

	/** The condition expression to evaluate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Precondition")
	FString ConditionExpression;

	/** Operator for comparison */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Precondition")
	EBehaviacOperatorType Operator;

	/** Left operand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Precondition")
	FString LeftOperand;

	/** Right operand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Precondition")
	FString RightOperand;
};

/**
 * Effector: applied after node completion (success or failure).
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIACRUNTIME_API UBehaviacEffector : public UBehaviacAttachment
{
	GENERATED_BODY()

public:
	UBehaviacEffector();

	virtual void Apply(UBehaviacAgentComponent* Agent, bool bSuccess) const override;

	/** The action expression to execute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Effector")
	FString ActionExpression;

	/** Property to set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Effector")
	FString PropertyName;

	/** Value to set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Effector")
	FString PropertyValue;
};

/**
 * Event attachment: triggers behavior based on named events.
 */
UCLASS(Blueprintable, EditInlineNew)
class BEHAVIACRUNTIME_API UBehaviacEventAttachment : public UBehaviacAttachment
{
	GENERATED_BODY()

public:
	UBehaviacEventAttachment();

	/** Name of the event to listen for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Event")
	FString EventName;

	/** Whether the event triggers once or repeatedly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Event")
	bool bTriggeredOnce;

	/** The subtree to run when event fires */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Event")
	FString ReferenceFilename;
};
