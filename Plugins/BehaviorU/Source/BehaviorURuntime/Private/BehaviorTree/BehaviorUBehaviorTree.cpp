// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviorUBehaviorTree.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/Composites/BehaviorUComposites.h"
#include "BehaviorTree/Actions/BehaviorUActions.h"
#include "BehaviorTree/Conditions/BehaviorUConditions.h"
#include "BehaviorTree/Decorators/BehaviorUDecorators.h"
#include "BehaviorTree/Attachments/BehaviorUAttachment.h"
#include "FSM/BehaviorUFSM.h"
#include "Misc/FileHelper.h"
#include "XmlFile.h"

UBehaviorUBehaviorTree::UBehaviorUBehaviorTree()
	: RootNode(nullptr)
	, Version(0)
{
}

// Forward declaration for recursive parsing
static UBehaviorUBehaviorNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer);

/** Map a class name to a node UClass */
static UBehaviorUBehaviorNode* CreateNodeByClassName(const FString& ClassName, UObject* Outer)
{
	// Composites
	if (ClassName == TEXT("Selector"))			return NewObject<UBehaviorUSelector>(Outer);
	if (ClassName == TEXT("Sequence"))			return NewObject<UBehaviorUSequence>(Outer);
	if (ClassName == TEXT("Parallel"))			return NewObject<UBehaviorUParallel>(Outer);
	if (ClassName == TEXT("IfElse"))				return NewObject<UBehaviorUIfElse>(Outer);
	if (ClassName == TEXT("SelectorLoop"))		return NewObject<UBehaviorUSelectorLoop>(Outer);
	if (ClassName == TEXT("SelectorProbability")) return NewObject<UBehaviorUSelectorProbability>(Outer);
	if (ClassName == TEXT("SelectorStochastic"))	return NewObject<UBehaviorUSelectorStochastic>(Outer);
	if (ClassName == TEXT("SequenceStochastic"))	return NewObject<UBehaviorUSequenceStochastic>(Outer);
	if (ClassName == TEXT("ReferencedBehavior"))	return NewObject<UBehaviorUReferenceBehavior>(Outer);
	if (ClassName == TEXT("WithPrecondition"))	return NewObject<UBehaviorUWithPrecondition>(Outer);

	// Actions
	if (ClassName == TEXT("Action"))				return NewObject<UBehaviorUAction>(Outer);
	if (ClassName == TEXT("Assignment"))			return NewObject<UBehaviorUAssignment>(Outer);
	if (ClassName == TEXT("Compute"))			return NewObject<UBehaviorUCompute>(Outer);
	if (ClassName == TEXT("Noop"))				return NewObject<UBehaviorUNoop>(Outer);
	if (ClassName == TEXT("End"))				return NewObject<UBehaviorUEnd>(Outer);
	if (ClassName == TEXT("Wait"))				return NewObject<UBehaviorUWait>(Outer);
	if (ClassName == TEXT("WaitFrames"))			return NewObject<UBehaviorUWaitFrames>(Outer);
	if (ClassName == TEXT("WaitforSignal"))		return NewObject<UBehaviorUWaitForSignal>(Outer);

	// Conditions
	if (ClassName == TEXT("Condition"))			return NewObject<UBehaviorUCondition>(Outer);
	if (ClassName == TEXT("And"))				return NewObject<UBehaviorUAnd>(Outer);
	if (ClassName == TEXT("Or"))					return NewObject<UBehaviorUOr>(Outer);
	if (ClassName == TEXT("True"))				return NewObject<UBehaviorUTrue>(Outer);
	if (ClassName == TEXT("False"))				return NewObject<UBehaviorUFalse>(Outer);

	// Decorators
	if (ClassName == TEXT("DecoratorAlwaysFailure"))		return NewObject<UBehaviorUDecoratorAlwaysFailure>(Outer);
	if (ClassName == TEXT("DecoratorAlwaysRunning"))		return NewObject<UBehaviorUDecoratorAlwaysRunning>(Outer);
	if (ClassName == TEXT("DecoratorAlwaysSuccess"))		return NewObject<UBehaviorUDecoratorAlwaysSuccess>(Outer);
	if (ClassName == TEXT("DecoratorNot"))				return NewObject<UBehaviorUDecoratorNot>(Outer);
	if (ClassName == TEXT("DecoratorLoop"))				return NewObject<UBehaviorUDecoratorLoop>(Outer);
	if (ClassName == TEXT("DecoratorLoopUntil"))			return NewObject<UBehaviorUDecoratorLoopUntil>(Outer);
	if (ClassName == TEXT("DecoratorRepeat"))			return NewObject<UBehaviorUDecoratorRepeat>(Outer);
	if (ClassName == TEXT("DecoratorCount"))				return NewObject<UBehaviorUDecoratorCount>(Outer);
	if (ClassName == TEXT("DecoratorCountLimit"))		return NewObject<UBehaviorUDecoratorCountLimit>(Outer);
	if (ClassName == TEXT("DecoratorTime"))				return NewObject<UBehaviorUDecoratorTime>(Outer);
	if (ClassName == TEXT("DecoratorFrames"))			return NewObject<UBehaviorUDecoratorFrames>(Outer);
	if (ClassName == TEXT("DecoratorFailureUntil"))		return NewObject<UBehaviorUDecoratorFailureUntil>(Outer);
	if (ClassName == TEXT("DecoratorSuccessUntil"))		return NewObject<UBehaviorUDecoratorSuccessUntil>(Outer);
	if (ClassName == TEXT("DecoratorIterator"))			return NewObject<UBehaviorUDecoratorIterator>(Outer);
	if (ClassName == TEXT("DecoratorLog"))				return NewObject<UBehaviorUDecoratorLog>(Outer);
	if (ClassName == TEXT("DecoratorWeight"))			return NewObject<UBehaviorUDecoratorWeight>(Outer);

	// FSM
	if (ClassName == TEXT("FSM"))				return NewObject<UBehaviorUFSMNode>(Outer);

	UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] Unknown node class: %s"), *ClassName);
	return nullptr;
}

static UBehaviorUBehaviorNode* ParseNodeFromXML(const FXmlNode* XmlNode, UObject* Outer)
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

	UBehaviorUBehaviorNode* BehaviorNode = CreateNodeByClassName(ClassName, Outer);
	if (!BehaviorNode)
	{
		return nullptr;
	}

	BehaviorNode->NodeClassName = ClassName;

	// Parse properties
	TArray<FBehaviorUProperty> Properties;
	for (const FXmlNode* PropNode : XmlNode->GetChildrenNodes())
	{
		if (PropNode->GetTag() == TEXT("property"))
		{
			Properties.Add(FBehaviorUProperty(
				PropNode->GetAttribute(TEXT("name")),
				PropNode->GetAttribute(TEXT("value"))
			));
		}
	}

	// Also parse inline attributes as properties
	FString IdAttr = XmlNode->GetAttribute(TEXT("id"));
	if (!IdAttr.IsEmpty())
	{
		Properties.Add(FBehaviorUProperty(TEXT("Id"), IdAttr));
	}

	BehaviorNode->LoadFromProperties(0, TEXT(""), Properties);

	// Parse child nodes
	for (const FXmlNode* ChildXml : XmlNode->GetChildrenNodes())
	{
		if (ChildXml->GetTag() == TEXT("node") || ChildXml->GetTag() == TEXT("custom"))
		{
			UBehaviorUBehaviorNode* ChildNode = ParseNodeFromXML(ChildXml, Outer);
			if (ChildNode)
			{
				BehaviorNode->AddChild(ChildNode);
			}
		}
		else if (ChildXml->GetTag() == TEXT("attachment"))
		{
			FString AttachClass = ChildXml->GetAttribute(TEXT("class"));

			// Parse attachment properties
			TArray<FBehaviorUProperty> AttachProps;
			for (const FXmlNode* AttachPropNode : ChildXml->GetChildrenNodes())
			{
				if (AttachPropNode->GetTag() == TEXT("property"))
				{
					AttachProps.Add(FBehaviorUProperty(
						AttachPropNode->GetAttribute(TEXT("name")),
						AttachPropNode->GetAttribute(TEXT("value"))
					));
				}
			}

			// Create appropriate attachment
			UBehaviorUAttachment* Attachment = nullptr;

			if (AttachClass.Contains(TEXT("Precondition")))
			{
				UBehaviorUPrecondition* Precond = NewObject<UBehaviorUPrecondition>(BehaviorNode);
				Precond->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Preconditions.Add(Precond);
			}
			else if (AttachClass.Contains(TEXT("Effector")))
			{
				UBehaviorUEffector* Eff = NewObject<UBehaviorUEffector>(BehaviorNode);
				Eff->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Effectors.Add(Eff);
			}
			else if (AttachClass.Contains(TEXT("Event")))
			{
				UBehaviorUEventAttachment* Evt = NewObject<UBehaviorUEventAttachment>(BehaviorNode);
				Evt->LoadFromProperties(0, TEXT(""), AttachProps);
				BehaviorNode->Events.Add(Evt);
			}
		}
	}

	return BehaviorNode;
}

bool UBehaviorUBehaviorTree::LoadFromXML(const FString& XMLContent)
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
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] Failed to parse XML content"));
		return false;
	}

	const FXmlNode* Root = XmlFile.GetRootNode();
	if (!Root)
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] XML has no root node"));
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
			BEHAVIORU_VLOG(TEXT("[BehaviorU] XML parsed! RootNode=%s, ChildCount=%d"), 
				*RootNode->GetName(), RootNode->GetChildCount());
		}
		else
		{
			UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] ParseNodeFromXML returned NULL!"));
		}
	}
	else
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] No <node> element found in XML!"));
	}

	return RootNode != nullptr;
}

// ===================================================================
// Blueprint Function Library
// ===================================================================

UBehaviorUBehaviorTree* UBehaviorUBehaviorTreeLibrary::LoadBehaviorTreeFromFile(UObject* WorldContext, const FString& FilePath)
{
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	UBehaviorUBehaviorTree* Tree = NewObject<UBehaviorUBehaviorTree>(GetTransientPackage());
	Tree->SourceFilePath = FilePath;
	Tree->TreeName = FPaths::GetBaseFilename(FilePath);

	if (Tree->LoadFromXML(FileContent))
	{
		return Tree;
	}

	UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] Failed to parse behavior tree from file: %s"), *FilePath);
	return nullptr;
}
