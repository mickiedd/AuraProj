// Copyright Druid Mechanics

#include "Commands/AuraConfigureCrunchAnimBlueprintCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AuraConfigureCrunchAnimBlueprintPrivate
{
	constexpr TCHAR BlueprintPath[] = TEXT("/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV4.ABP_Crunch_AuraV4");
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

	void WriteReport(bool bConfigured, const FString& Error, int32 SlotNodes, bool bLinked)
	{
		const FString AbsolutePath = FPaths::ProjectDir() / ReportPath;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		const FString Json = FString::Printf(
			TEXT("{\n  \"configured\": %s,\n  \"slotNodes\": %d,\n  \"linkedToOutput\": %s,\n  \"error\": \"%s\"\n}\n"),
			bConfigured ? TEXT("true") : TEXT("false"), SlotNodes, bLinked ? TEXT("true") : TEXT("false"), *Error.ReplaceCharWithEscapedChar());
		FFileHelper::SaveStringToFile(Json, *AbsolutePath);
	}
}

int32 UAuraConfigureCrunchAnimBlueprintCommandlet::Main(const FString& Params)
{
	using namespace AuraConfigureCrunchAnimBlueprintPrivate;
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	if (!Blueprint)
	{
		WriteReport(false, TEXT("AnimBlueprint failed to load"), 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] %s"), TEXT("AnimBlueprint failed to load"));
		return 1;
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
		WriteReport(false, TEXT("AnimGraph failed to load"), 0, false);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] %s"), TEXT("AnimGraph failed to load"));
		return 1;
	}

	TArray<UAnimGraphNode_Base*> SlotBaseNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Slot::StaticClass(), SlotBaseNodes);
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	for (UAnimGraphNode_Base* Node : SlotBaseNodes)
	{
		if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node)) SlotNodes.Add(Slot);
	}
	if (SlotNodes.Num() == 0)
	{
		TArray<UAnimGraphNode_Base*> RootBaseNodes;
		AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootBaseNodes);
		TArray<UAnimGraphNode_Root*> RootNodes;
		for (UAnimGraphNode_Base* Node : RootBaseNodes)
		{
			if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node)) RootNodes.Add(Root);
		}
		if (RootNodes.Num() != 1 || !RootNodes[0])
		{
			WriteReport(false, TEXT("AnimGraph does not have exactly one output root"), 0, false);
			UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Missing unique output root."));
			return 1;
		}

		UAnimGraphNode_Slot* SlotNode = NewObject<UAnimGraphNode_Slot>(AnimationGraph, NAME_None, RF_Transactional);
		if (!SlotNode)
		{
			WriteReport(false, TEXT("Failed to allocate slot node"), 0, false);
			return 1;
		}
		SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
		SlotNode->Node.bAlwaysUpdateSourcePose = true;
		SlotNode->CreateNewGuid();
		AnimationGraph->AddNode(SlotNode, true, false);
		SlotNode->AllocateDefaultPins();
		UEdGraphPin* RootResult = RootNodes[0]->FindPin(TEXT("Result"), EGPD_Input);
		UEdGraphPin* SlotPose = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
		if (!RootResult || !SlotPose)
		{
			WriteReport(false, TEXT("Slot or output pose pin unavailable"), 1, false);
			UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Slot/output pins unavailable."));
			return 1;
		}
		SlotPose->MakeLinkTo(RootResult);
		AnimationGraph->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SlotNodes.Add(SlotNode);
	}

	bool bLinked = false;
	for (UAnimGraphNode_Slot* SlotNode : SlotNodes)
	{
		if (!SlotNode) continue;
		bLinked |= SlotNode->Node.SlotName == FName(TEXT("DefaultSlot"));
	}
	if (!SaveBlueprint(Blueprint))
	{
		WriteReport(false, TEXT("Failed to save configured AnimBlueprint"), SlotNodes.Num(), bLinked);
		UE_LOG(LogTemp, Error, TEXT("[CrunchAnimBlueprint] Failed to save configured AnimBlueprint."));
		return 1;
	}
	WriteReport(true, TEXT(""), SlotNodes.Num(), bLinked);
	UE_LOG(LogTemp, Display, TEXT("[CrunchAnimBlueprint] PASS Asset=%s SlotNodes=%d DefaultSlot=%d"),
		BlueprintPath, SlotNodes.Num(), bLinked ? 1 : 0);
	return 0;
}
