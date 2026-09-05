// Copyright Druid Mechanics

#include "Commands/AuraConfigureCivilianAnimBlueprintCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AuraConfigureCivilianAnimBlueprintPrivate
{
	constexpr TCHAR BlueprintPath[] = TEXT("/Game/Blueprints/Character/Shaman/ABP_Shaman.ABP_Shaman");
	constexpr TCHAR ReportPath[] = TEXT("Saved/Reports/Civilian/civilian-anim-blueprint-configure.json");
	constexpr TCHAR UpdateFunctionName[] = TEXT("BlueprintUpdateAnimation");

	void WriteReport(bool bConfigured, bool bEventFound, bool bParentCallFound, bool bEventEnabled, bool bParentCallEnabled, const FString& Error)
	{
		const FString AbsolutePath = FPaths::ProjectDir() / ReportPath;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		const FString Json = FString::Printf(
			TEXT("{\n  \"configured\": %s,\n  \"updateEventFound\": %s,\n  \"parentCallFound\": %s,\n  \"updateEventEnabled\": %s,\n  \"parentCallEnabled\": %s,\n  \"error\": \"%s\"\n}\n"),
			bConfigured ? TEXT("true") : TEXT("false"),
			bEventFound ? TEXT("true") : TEXT("false"),
			bParentCallFound ? TEXT("true") : TEXT("false"),
			bEventEnabled ? TEXT("true") : TEXT("false"),
			bParentCallEnabled ? TEXT("true") : TEXT("false"),
			*Error.ReplaceCharWithEscapedChar());
		FFileHelper::SaveStringToFile(Json, *AbsolutePath);
	}

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
}

int32 UAuraConfigureCivilianAnimBlueprintCommandlet::Main(const FString& Params)
{
	using namespace AuraConfigureCivilianAnimBlueprintPrivate;
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	if (!Blueprint)
	{
		WriteReport(false, false, false, false, false, TEXT("Civilian AnimBlueprint failed to load"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] Failed to load %s"), BlueprintPath);
		return 1;
	}

	UEdGraph* EventGraph = nullptr;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == TEXT("EventGraph"))
		{
			EventGraph = Graph;
			break;
		}
	}
	if (!EventGraph)
	{
		WriteReport(false, false, false, false, false, TEXT("EventGraph failed to load"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] EventGraph missing from %s"), BlueprintPath);
		return 1;
	}

	UK2Node_Event* UpdateEvent = nullptr;
	TArray<UK2Node_Event*> EventNodes;
	EventGraph->GetNodesOfClass(EventNodes);
	for (UK2Node_Event* EventNode : EventNodes)
	{
		if (EventNode && EventNode->GetFunctionName() == FName(UpdateFunctionName))
		{
			UpdateEvent = EventNode;
			break;
		}
	}

	UK2Node_CallParentFunction* ParentCall = nullptr;
	TArray<UK2Node_CallParentFunction*> ParentCallNodes;
	EventGraph->GetNodesOfClass(ParentCallNodes);
	for (UK2Node_CallParentFunction* ParentCallNode : ParentCallNodes)
	{
		if (ParentCallNode && ParentCallNode->GetFunctionName() == FName(UpdateFunctionName))
		{
			ParentCall = ParentCallNode;
			break;
		}
	}

	if (!UpdateEvent || !ParentCall)
	{
		WriteReport(false, UpdateEvent != nullptr, ParentCall != nullptr,
			UpdateEvent && UpdateEvent->GetDesiredEnabledState() == ENodeEnabledState::Enabled,
			ParentCall && ParentCall->GetDesiredEnabledState() == ENodeEnabledState::Enabled,
			TEXT("BlueprintUpdateAnimation event or parent call is missing"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] Missing BlueprintUpdateAnimation event or parent call."));
		return 1;
	}

	UpdateEvent->SetEnabledState(ENodeEnabledState::Enabled);
	ParentCall->SetEnabledState(ENodeEnabledState::Enabled);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		WriteReport(false, true, true, UpdateEvent->GetDesiredEnabledState() == ENodeEnabledState::Enabled,
			ParentCall->GetDesiredEnabledState() == ENodeEnabledState::Enabled,
			TEXT("Civilian AnimBlueprint failed to compile"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] Compile failed for %s"), BlueprintPath);
		return 1;
	}

	const bool bEventEnabled = UpdateEvent->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
	const bool bParentCallEnabled = ParentCall->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
	if (!bEventEnabled || !bParentCallEnabled || !SaveBlueprint(Blueprint))
	{
		WriteReport(false, true, true, bEventEnabled, bParentCallEnabled,
			(!bEventEnabled || !bParentCallEnabled) ? TEXT("movement update path did not enable") : TEXT("failed to save Civilian AnimBlueprint"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] Failed to enable and save movement update path."));
		return 1;
	}

	WriteReport(true, true, true, true, true, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("[CivilianAnimBlueprint] PASS Asset=%s BlueprintUpdateAnimation=enabled ParentCall=enabled"), BlueprintPath);
	return 0;
}
