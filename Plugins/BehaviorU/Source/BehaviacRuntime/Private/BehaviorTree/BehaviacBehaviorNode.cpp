// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"

UBehaviacBehaviorNode::UBehaviacBehaviorNode()
	: NodeId(BEHAVIAC_INVALID_NODE_ID)
	, bHasEvents(false)
	, ParentNode(nullptr)
{
}

void UBehaviacBehaviorNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	AgentType = InAgentType;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Id"))
		{
			NodeId = FCString::Atoi(*Prop.Value);
		}
	}
}

void UBehaviacBehaviorNode::AddChild(UBehaviacBehaviorNode* Child)
{
	if (Child)
	{
		Child->SetParent(this);
		Children.Add(Child);
	}
}

UBehaviacBehaviorTask* UBehaviacBehaviorNode::CreateTask(UObject* Outer) const
{
	// Base class returns nullptr; subclasses override to create their specific task type
	return nullptr;
}

bool UBehaviacBehaviorNode::IsValid(UBehaviacAgentComponent* Agent, UBehaviacBehaviorTask* Task) const
{
	return Agent != nullptr;
}

UBehaviacBehaviorNode* UBehaviacBehaviorNode::GetChild(int32 Index) const
{
	if (Children.IsValidIndex(Index))
	{
		return Children[Index];
	}
	return nullptr;
}
