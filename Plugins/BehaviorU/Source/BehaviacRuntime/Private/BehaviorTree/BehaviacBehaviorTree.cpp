// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviacBehaviorTree.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/Composites/BehaviacComposites.h"
#include "BehaviorTree/Actions/BehaviacActions.h"
#include "BehaviorTree/Conditions/BehaviacConditions.h"
#include "BehaviorTree/Decorators/BehaviacDecorators.h"
#include "BehaviorTree/Attachments/BehaviacAttachment.h"
#include "FSM/BehaviacFSM.h"
#include "Misc/FileHelper.h"
#include "XmlFile.h"

UBehaviacBehaviorTree::UBehaviacBehaviorTree()
	: RootNode(nullptr)
	, Version(0)
{
}

// Forward declaration for recursive parsing
static UBehaviacBehaviorNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer);

/** Map a class name to a node UClass */
static UBehaviacBehaviorNode* CreateNodeByClassName(const FString& ClassName, UObject* Outer)
{
	// Composites
	if (ClassName == TEXT("Selector"))			return NewObject<UBehaviacSelector>(Outer);
	if (ClassName == TEXT("Sequence"))			return NewObject<UBehaviacSequence>(Outer);
	if (ClassName == TEXT("Parallel"))			return NewObject<UBehaviacParallel>(Outer);
	if (ClassName == TEXT("IfElse"))				return NewObject<UBehaviacIfElse>(Outer);
	if (ClassName == TEXT("SelectorLoop"))		return NewObject<UBehaviacSelectorLoop>(Outer);
	if (ClassName == TEXT("SelectorProbability")) return NewObject<UBehaviacSelectorProbability>(Outer);
	if (ClassName == TEXT("SelectorStochastic"))	return NewObject<UBehaviacSelectorStochastic>(Outer);
	if (ClassName == TEXT("SequenceStochastic"))	return NewObject<UBehaviacSequenceStochastic>(Outer);
	if (ClassName == TEXT("ReferencedBehavior"))	return NewObject<UBehaviacReferenceBehavior>(Outer);
	if (ClassName == TEXT("WithPrecondition"))	return NewObject<UBehaviacWithPrecondition>(Outer);

	// Actions
	if (ClassName == TEXT("Action"))				return NewObject<UBehaviacAction>(Outer);
	if (ClassName == TEXT("Assignment"))			return NewObject<UBehaviacAssignment>(Outer);
	if (ClassName == TEXT("Compute"))			return NewObject<UBehaviacCompute>(Outer);
	if (ClassName == TEXT("Noop"))				return NewObject<UBehaviacNoop>(Outer);
	if (ClassName == TEXT("End"))				return NewObject<UBehaviacEnd>(Outer);
	if (ClassName == TEXT("Wait"))				return NewObject<UBehaviacWait>(Outer);
	if (ClassName == TEXT("WaitFrames"))			return NewObject<UBehaviacWaitFrames>(Outer);
	if (ClassName == TEXT("WaitforSignal"))		return NewObject<UBehaviacWaitForSignal>(Outer);

	// Conditions
	if (ClassName == TEXT("Condition"))			return NewObject<UBehaviacCondition>(Outer);
	if (ClassName == TEXT("And"))				return NewObject<UBehaviacAnd>(Outer);
	if (ClassName == TEXT("Or"))					return NewObject<UBehaviacOr>(Outer);
	if (ClassName == TEXT("True"))				return NewObject<UBehaviacTrue>(Outer);
	if (ClassName == TEXT("False"))				return NewObject<UBehaviacFalse>(Outer);

	// Decorators
	if (ClassName == TEXT("DecoratorAlwaysFailure"))		return NewObject<UBehaviacDecoratorAlwaysFailure>(Outer);
	if (ClassName == TEXT("DecoratorAlwaysRunning"))		return NewObject<UBehaviacDecoratorAlwaysRunning>(Outer);
	if (ClassName == TEXT("DecoratorAlwaysSuccess"))		return NewObject<UBehaviacDecoratorAlwaysSuccess>(Outer);
	if (ClassName == TEXT("DecoratorNot"))				return NewObject<UBehaviacDecoratorNot>(Outer);
	if (ClassName == TEXT("DecoratorLoop"))				return NewObject<UBehaviacDecoratorLoop>(Outer);
	if (ClassName == TEXT("DecoratorLoopUntil"))			return NewObject<UBehaviacDecoratorLoopUntil>(Outer);
	if (ClassName == TEXT("DecoratorRepeat"))			return NewObject<UBehaviacDecoratorRepeat>(Outer);
	if (ClassName == TEXT("DecoratorCount"))				return NewObject<UBehaviacDecoratorCount>(Outer);
	if (ClassName == TEXT("DecoratorCountLimit"))		return NewObject<UBehaviacDecoratorCountLimit>(Outer);
	if (ClassName == TEXT("DecoratorTime"))				return NewObject<UBehaviacDecoratorTime>(Outer);
	if (ClassName == TEXT("DecoratorFrames"))			return NewObject<UBehaviacDecoratorFrames>(Outer);
	if (ClassName == TEXT("DecoratorFailureUntil"))		return NewObject<UBehaviacDecoratorFailureUntil>(Outer);
	if (ClassName == TEXT("DecoratorSuccessUntil"))		return NewObject<UBehaviacDecoratorSuccessUntil>(Outer);
	if (ClassName == TEXT("DecoratorIterator"))			return NewObject<UBehaviacDecoratorIterator>(Outer);
	if (ClassName == TEXT("DecoratorLog"))				return NewObject<UBehaviacDecoratorLog>(Outer);
	if (ClassName == TEXT("DecoratorWeight"))			return NewObject<UBehaviacDecoratorWeight>(Outer);

	// FSM
	if (ClassName == TEXT("FSM"))				return NewObject<UBehaviacFSMNode>(Outer);

	UE_LOG(LogBehaviac, Warning, TEXT("[Behaviac] Unknown node class: %s"), *ClassName);
	return nullptr;
}

static UBehaviacBehaviorNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer)
{
	if (!XmlNode)
	{
		return nullptr;
	}

	FString ClassName = XmlNode->GetAttribute(TEXT("class"));
	if (ClassName.IsEmpty())
	{
		ClassName = XmlNode->GetTag();
	}

	// Strip namespace prefix (e.g., "behaviac::Selector" -> "Selector")
	int32 LastColonIdx;
	if (ClassName.FindLastChar(TEXT(':'), LastColonIdx))
	{
		ClassName = ClassName.Mid(LastColonIdx + 1);
	}

	UBehaviacBehaviorNode* BehaviorNode = CreateNodeByClassName(ClassName, Outer);
	if (!BehaviorNode)
	{
		return nullptr;
	}

	BehaviorNode->NodeClassName = ClassName;

	// Parse properties
	TArray<FBehaviacProperty> Properties;
	for (const FXmlNode* PropNode : XmlNode->GetChildrenNodes())
	{
		if (PropNode->GetTag() == TEXT("property"))
		{
			Properties.Add(FBehaviacProperty(
				PropNode->GetAttribute(TEXT("name")),
				PropNode->GetAttribute(TEXT("value"))
			));
		}
	}

	// Also parse inline attributes as properties
	FString IdAttr = XmlNode->GetAttribute(TEXT("id"));
	if (!IdAttr.IsEmpty())
	{
		Properties.Add(FBehaviacProperty(TEXT("Id"), IdAttr));
	}

	BehaviorNode->LoadFromProperties(0, TEXT(""), Properties);

	// Parse child nodes
	for (const FXmlNode* ChildXml : XmlNode->GetChildrenNodes())
	{
		if (ChildXml->GetTag() == TEXT("node") || ChildXml->GetTag() == TEXT("custom"))
		{
			UBehaviacBehaviorNode* ChildNode = ParseNodeFromXML(ChildXml, Outer);
			if (ChildNode)
			{
				BehaviorNode->AddChild(ChildNode);
			}
		}
		else if (ChildXml->GetTag() == TEXT("attachment"))
		{
			FString AttachClass = ChildXml->GetAttribute(TEXT("class"));

			// Parse attachment properties
			TArray<FBehaviacProperty> AttachProps;
			for (const FXmlNode* AttachPropNode : ChildXml->GetChildrenNodes())
			{
				if (AttachPropNode->GetTag() == TEXT("property"))
				{
					AttachProps.Add(FBehaviacProperty(
						AttachPropNode->GetAttribute(TEXT("name")),
						AttachPropNode->GetAttribute(TEXT("value"))
					));
				}
			}

			// Create appropriate attachment
			UBehaviacAttachment* Attachment = nullptr;

			if (AttachClass.Contains(TEXT("Precondition")))
			{
				UBehaviacPrecondition* Precond = NewObject<UBehaviacPrecondition>(BehaviorNode);
				Precond->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Preconditions.Add(Precond);
			}
			else if (AttachClass.Contains(TEXT("Effector")))
			{
				UBehaviacEffector* Eff = NewObject<UBehaviacEffector>(BehaviorNode);
				Eff->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Effectors.Add(Eff);
			}
			else if (AttachClass.Contains(TEXT("Event")))
			{
				UBehaviacEventAttachment* Evt = NewObject<UBehaviacEventAttachment>(BehaviorNode);
				Evt->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Events.Add(Evt);
			}
		}
	}

	return BehaviorNode;
}

bool UBehaviacBehaviorTree::LoadFromXML(const FString& XMLContent)
{
	// 缓存原始 XML，供调试服务器在客户端连接时推送
	SourceXML = XMLContent;

	// FXmlFile's ConstructFromBuffer splits on newlines before stripping <?xml?>.
	// If the entire XML is on one line, the prolog removal blanks the whole string.
	// Fix: strip the XML declaration ourselves and add a newline after every '>'.
	FString Sanitized = XMLContent;
	// Remove the <?xml ... ?> prolog (handles both single-line and multi-line)
	{
		int32 PrologStart = Sanitized.Find(TEXT("<?xml"), ESearchCase::IgnoreCase);
		if (PrologStart != INDEX_NONE)
		{
			int32 PrologEnd = Sanitized.Find(TEXT("?>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PrologStart);
			if (PrologEnd != INDEX_NONE)
			{
				Sanitized.RemoveAt(PrologStart, (PrologEnd + 2) - PrologStart);
			}
		}
	}
	// Ensure each tag ends with a newline so FXmlFile can parse line-by-line
	Sanitized = Sanitized.Replace(TEXT(">"), TEXT(">\n"));

	FXmlFile XmlFile(Sanitized, EConstructMethod::ConstructFromBuffer);

	if (!XmlFile.IsValid())
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[Behaviac] Failed to parse XML content"));
		return false;
	}

	const FXmlNode* Root = XmlFile.GetRootNode();
	if (!Root)
	{
		UE_LOG(LogBehaviac, Error, TEXT("[Behaviac] XML has no root node"));
		return false;
	}

	// Parse version
	FString VersionStr = Root->GetAttribute(TEXT("version"));
	if (!VersionStr.IsEmpty())
	{
		Version = FCString::Atoi(*VersionStr);
	}

	// Parse agent type
	AgentType = Root->GetAttribute(TEXT("agenttype"));

	// Find the first node element
	const FXmlNode* FirstNode = Root->FindChildNode(TEXT("node"));
	if (!FirstNode)
	{
		// Try direct children
		for (const FXmlNode* Child : Root->GetChildrenNodes())
		{
			if (Child->GetAttribute(TEXT("class")).Len() > 0)
			{
				FirstNode = Child;
				break;
			}
		}
	}

	if (FirstNode)
	{
		RootNode = ParseNodeFromXML(FirstNode, this);
		
		if (RootNode)
		{
			BEHAVIAC_VLOG(TEXT("[Behaviac] XML parsed! RootNode=%s, ChildCount=%d"), 
				*RootNode->GetName(), RootNode->GetChildCount());
		}
		else
		{
			UE_LOG(LogBehaviac, Error, TEXT("[Behaviac] ParseNodeFromXML returned NULL!"));
		}
	}
	else
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[Behaviac] No <node> element found in XML!"));
	}

	return RootNode != nullptr;
}

// ===================================================================
// Blueprint Function Library
// ===================================================================

UBehaviacBehaviorTree* UBehaviacBehaviorTreeLibrary::LoadBehaviorTreeFromFile(UObject* WorldContext, const FString& FilePath)
{
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogBehaviac, Error, TEXT("[Behaviac] Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	UBehaviacBehaviorTree* Tree = NewObject<UBehaviacBehaviorTree>(GetTransientPackage());
	Tree->SourceFilePath = FilePath;
	Tree->TreeName = FPaths::GetBaseFilename(FilePath);

	if (Tree->LoadFromXML(FileContent))
	{
		return Tree;
	}

	UE_LOG(LogBehaviac, Error, TEXT("[Behaviac] Failed to parse behavior tree from file: %s"), *FilePath);
	return nullptr;
}
