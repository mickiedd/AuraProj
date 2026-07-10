// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"

UBehaviorUBehaviorNode::UBehaviorUBehaviorNode()
	: NodeId(BEHAVIORU_INVALID_NODE_ID)
	, bHasEvents(false)
	, ParentNode(nullptr)
{
}

void UBehaviorUBehaviorNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	AgentType = InAgentType;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Id"))
		{
			NodeId = FCString::Atoi(*Prop.Value);
		}
	}
}

void UBehaviorUBehaviorNode::AddChild(UBehaviorUBehaviorNode* Child)
{
	if (Child)
	{
		Child->SetParent(this);
		Children.Add(Child);
	}
}

UBehaviorUBehaviorTask* UBehaviorUBehaviorNode::CreateTask(UObject* Outer) const
{
	// Base class returns nullptr; subclasses override to create their specific task type
	return nullptr;
}

bool UBehaviorUBehaviorNode::IsValid(UBehaviorUAgentComponent* Agent, UBehaviorUBehaviorTask* Task) const
{
	return Agent != nullptr;
}

UBehaviorUBehaviorNode* UBehaviorUBehaviorNode::GetChild(int32 Index) const
{
	if (Children.IsValidIndex(Index))
	{
		return Children[Index];
	}
	return nullptr;
}
