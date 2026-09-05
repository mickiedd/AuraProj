// Copyright Druid Mechanics

#include "Commands/AuraConfigureCrunchAnimBlueprintCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AuraCharacterAnimInstance.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AuraConfigureCrunchAnimBlueprintPrivate
{
	constexpr TCHAR BlueprintPath[] = TEXT("/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV5.ABP_Crunch_AuraV5");
	constexpr TCHAR IdleSequencePath[] = TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Idle_CombatV4.Idle_CombatV4");
	constexpr TCHAR JogSequencePath[] = TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Jog_FwdV4.Jog_FwdV4");
	constexpr TCHAR ReportPath[] = TEXT("Saved/Reports/CrunchMigration/anim-blueprint-configure.json");

	bool SaveBlueprint(UAnimBlueprint* Blueprint)
	{
		if (!Blueprint || !Blueprint->GetPackage()) return false;
		Blueprint->GetPackage()->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs);
	}

	void WriteReport(bool bConfigured, const FString& Error, int32 SlotNodes, int32 SequencePlayers, bool bLocomotionLinked)
	{
		const FString AbsolutePath = FPaths::ProjectDir() / ReportPath;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		const FString Json = FString::Printf(
			TEXT("{\n  \"configured\": %s,\n  \"slotNodes\": %d,\n  \"sequencePlayers\": %d,\n  \"locomotionLinked\": %s,\n  \"error\": \"%s\"\n}\n"),
			bConfigured ? TEXT("true") : TEXT("false"), SlotNodes, SequencePlayers,
			bLocomotionLinked ? TEXT("true") : TEXT("false"), *Error.ReplaceCharWithEscapedChar());
		FFileHelper::SaveStringToFile(Json, *AbsolutePath);
	}

	bool LinkPins(const UEdGraphSchema* Schema, UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		return Schema && OutputPin && InputPin && Schema->TryCreateConnection(OutputPin, InputPin);
	}

	void LogPins(const TCHAR* Label, const UEdGraphNode* Node)
	{
		if (!Node) return;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				UE_LOG(LogTemp, Display, TEXT("[CrunchAnimBlueprint] %s Pin=%s Direction=%d"),
					Label, *Pin->PinName.ToString(), static_cast<int32>(Pin->Direction));
			}
		}
	}
}

int32 UAuraConfigureCrunchAnimBlueprintCommandlet::Main(const FString& Params)
{
	using namespace AuraConfigureCrunchAnimBlueprintPrivate;
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	if (!Blueprint)
	{
		WriteReport(false, TEXT("AnimBlueprint failed to load"), 0, 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] %s"), TEXT("AnimBlueprint failed to load"));
		return 1;
	}
	if (Blueprint->ParentClass != UAuraCharacterAnimInstance::StaticClass())
	{
		Blueprint->ParentClass = UAuraCharacterAnimInstance::StaticClass();
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UAnimationGraph* AnimationGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimationGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}
	if (!AnimationGraph)
	{
		WriteReport(false, TEXT("AnimGraph failed to load"), 0, 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] %s"), TEXT("AnimGraph failed to load"));
		return 1;
	}

	UAnimSequence* IdleSequence = LoadObject<UAnimSequence>(nullptr, IdleSequencePath);
	UAnimSequence* JogSequence = LoadObject<UAnimSequence>(nullptr, JogSequencePath);
	if (!IdleSequence || !JogSequence)
	{
		WriteReport(false, TEXT("Crunch locomotion sequences failed to load"), 0, 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Crunch locomotion sequences failed to load."));
		return 1;
	}

	TArray<UAnimGraphNode_Base*> RootBaseNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootBaseNodes);
	if (RootBaseNodes.Num() != 1)
	{
		WriteReport(false, TEXT("AnimGraph does not have exactly one output root"), 0, 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Missing unique output root."));
		return 1;
	}
	UAnimGraphNode_Root* RootNode = CastChecked<UAnimGraphNode_Root>(RootBaseNodes[0]);

	TArray<UAnimGraphNode_Base*> SlotBaseNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Slot::StaticClass(), SlotBaseNodes);
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	for (UAnimGraphNode_Base* Node : SlotBaseNodes)
	{
		if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node)) SlotNodes.Add(Slot);
	}
	if (SlotNodes.Num() == 0)
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> SlotCreator(*AnimationGraph);
		UAnimGraphNode_Slot* SlotNode = SlotCreator.CreateNode();
		SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
		SlotNode->Node.bAlwaysUpdateSourcePose = true;
		SlotCreator.Finalize();
		UEdGraphPin* RootResult = RootNode->FindPin(TEXT("Result"), EGPD_Input);
		UEdGraphPin* SlotPose = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
		if (!LinkPins(AnimationGraph->GetSchema(), SlotPose, RootResult))
		{
			WriteReport(false, TEXT("Failed to connect DefaultSlot to output"), 1, 0, false);
			UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Failed to connect DefaultSlot to output."));
			return 1;
		}
		SlotNodes.Add(SlotNode);
	}
	if (SlotNodes.Num() != 1)
	{
		WriteReport(false, TEXT("AnimGraph must have exactly one montage slot"), SlotNodes.Num(), 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Expected one montage slot, found %d."), SlotNodes.Num());
		return 1;
	}

	UAnimGraphNode_Slot* SlotNode = SlotNodes[0];
	SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
	SlotNode->Node.bAlwaysUpdateSourcePose = true;

	// The first migration created only Slot -> Output. Rebuild the generated
	// locomotion slice on every run so this commandlet stays deterministic.
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : AnimationGraph->Nodes)
	{
		if (Node && (Node->IsA<UAnimGraphNode_SequencePlayer>() || Node->IsA<UAnimGraphNode_BlendListByBool>()
			|| Node->IsA<UK2Node_VariableGet>()))
		{
			NodesToRemove.Add(Node);
		}
	}
	for (UEdGraphNode* Node : NodesToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}

	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> IdleCreator(*AnimationGraph);
	UAnimGraphNode_SequencePlayer* IdlePlayer = IdleCreator.CreateNode();
	IdlePlayer->Node.SetSequence(IdleSequence);
	IdlePlayer->NodePosX = -900;
	IdlePlayer->NodePosY = 100;
	IdleCreator.Finalize();

	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> JogCreator(*AnimationGraph);
	UAnimGraphNode_SequencePlayer* JogPlayer = JogCreator.CreateNode();
	JogPlayer->Node.SetSequence(JogSequence);
	JogPlayer->NodePosX = -900;
	JogPlayer->NodePosY = -100;
	JogCreator.Finalize();

	FGraphNodeCreator<UAnimGraphNode_BlendListByBool> BlendCreator(*AnimationGraph);
	UAnimGraphNode_BlendListByBool* MovementBlend = BlendCreator.CreateNode();
	MovementBlend->NodePosX = -600;
	MovementBlend->NodePosY = 0;
	BlendCreator.Finalize();

	FEdGraphSchemaAction_K2NewNode ShouldMoveAction;
	UK2Node_VariableGet* ShouldMoveTemplate = NewObject<UK2Node_VariableGet>();
	ShouldMoveTemplate->VariableReference.SetSelfMember(FName(TEXT("bShouldMove")));
	ShouldMoveAction.NodeTemplate = ShouldMoveTemplate;
	UK2Node_VariableGet* ShouldMove = Cast<UK2Node_VariableGet>(
		ShouldMoveAction.PerformAction(AnimationGraph, nullptr, FVector2D(-900.f, -250.f), true));
	if (!ShouldMove)
	{
		WriteReport(false, TEXT("Failed to create bShouldMove variable getter"), SlotNodes.Num(), 2, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Failed to create bShouldMove variable getter."));
		return 1;
	}

	SlotNode->NodePosX = -250;
	SlotNode->NodePosY = 0;
	RootNode->NodePosX = 100;
	RootNode->NodePosY = 0;

	const UEdGraphSchema* Schema = AnimationGraph->GetSchema();
	UEdGraphPin* IdlePose = IdlePlayer->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* JogPose = JogPlayer->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* TruePose = MovementBlend->FindPin(TEXT("BlendPose_0"), EGPD_Input);
	UEdGraphPin* FalsePose = MovementBlend->FindPin(TEXT("BlendPose_1"), EGPD_Input);
	UEdGraphPin* ActiveValue = MovementBlend->FindPin(TEXT("bActiveValue"), EGPD_Input);
	UEdGraphPin* ShouldMoveValue = ShouldMove->FindPin(TEXT("bShouldMove"), EGPD_Output);
	UEdGraphPin* BlendPose = MovementBlend->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* SlotSource = SlotNode->FindPin(TEXT("Source"), EGPD_Input);
	UEdGraphPin* SlotPose = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* RootResult = RootNode->FindPin(TEXT("Result"), EGPD_Input);
	if (SlotSource) SlotSource->BreakAllPinLinks();
	if (RootResult) RootResult->BreakAllPinLinks();
	const bool bLinked = LinkPins(Schema, JogPose, TruePose)
		&& LinkPins(Schema, IdlePose, FalsePose)
		&& LinkPins(Schema, ShouldMoveValue, ActiveValue)
		&& LinkPins(Schema, BlendPose, SlotSource)
		&& LinkPins(Schema, SlotPose, RootResult);
	if (!bLinked)
	{
		LogPins(TEXT("Idle"), IdlePlayer);
		LogPins(TEXT("Jog"), JogPlayer);
		LogPins(TEXT("Blend"), MovementBlend);
		LogPins(TEXT("ShouldMove"), ShouldMove);
		LogPins(TEXT("Slot"), SlotNode);
		WriteReport(false, TEXT("Failed to wire idle/jog locomotion graph"), SlotNodes.Num(), 2, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Failed to wire idle/jog locomotion graph."));
		return 1;
	}

	AnimationGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		WriteReport(false, TEXT("Configured AnimBlueprint failed to compile"), SlotNodes.Num(), 2, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Configured AnimBlueprint failed to compile."));
		return 1;
	}

	for (UAnimGraphNode_Slot* ConfiguredSlotNode : SlotNodes)
	{
		if (!ConfiguredSlotNode) continue;
		if (ConfiguredSlotNode->Node.SlotName != FName(TEXT("DefaultSlot")))
		{
			WriteReport(false, TEXT("Configured slot is not DefaultSlot"), SlotNodes.Num(), 2, false);
			return 1;
		}
	}
	if (!SaveBlueprint(Blueprint))
	{
		WriteReport(false, TEXT("Failed to save configured AnimBlueprint"), SlotNodes.Num(), 2, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Failed to save configured AnimBlueprint."));
		return 1;
	}
	WriteReport(true, TEXT(""), SlotNodes.Num(), 2, true);
	UE_LOG(LogTemp, Display, TEXT("[CrunchAnimBlueprint] PASS Asset=%s SlotNodes=%d SequencePlayers=2 LocomotionLinked=1"),
		BlueprintPath, SlotNodes.Num());
	return 0;
}
