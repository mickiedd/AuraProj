// Copyright Druid Mechanics

#include "Commands/AuraConfigureCivilianAnimBlueprintCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Character/AuraCharacterBase.h"
#include "Animation/AuraAnimationDiagnosticsLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
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
	constexpr TCHAR EnemyBlueprintPath[] = TEXT("/Game/Blueprints/Character/ABP_Enemy.ABP_Enemy");
	constexpr TCHAR ReportPath[] = TEXT("Saved/Reports/Civilian/civilian-anim-blueprint-configure.json");
	constexpr TCHAR UpdateFunctionName[] = TEXT("BlueprintUpdateAnimation");
	constexpr TCHAR InitializeFunctionName[] = TEXT("BlueprintInitializeAnimation");

	bool ConfigureEnemyMovementOwner(UAnimBlueprint* Blueprint, FString& OutError)
	{
		if (!Blueprint)
		{
			OutError = TEXT("Enemy AnimBlueprint failed to load");
			return false;
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
			OutError = TEXT("Enemy EventGraph failed to load");
			return false;
		}

		UK2Node_Event* InitializeEvent = nullptr;
		TArray<UK2Node_Event*> EventNodes;
		EventGraph->GetNodesOfClass(EventNodes);
		for (UK2Node_Event* EventNode : EventNodes)
		{
			if (EventNode && EventNode->GetFunctionName() == FName(InitializeFunctionName))
			{
				InitializeEvent = EventNode;
				break;
			}
		}

		UK2Node_CallFunction* TryGetPawnOwner = nullptr;
		TArray<UK2Node_CallFunction*> FunctionNodes;
		EventGraph->GetNodesOfClass(FunctionNodes);
		for (UK2Node_CallFunction* FunctionNode : FunctionNodes)
		{
			if (FunctionNode && FunctionNode->GetFunctionName() == FName(TEXT("TryGetPawnOwner")))
			{
				TryGetPawnOwner = FunctionNode;
				break;
			}
		}

		UK2Node_VariableGet* MovementGetter = nullptr;
		TArray<UK2Node_VariableGet*> VariableGets;
		EventGraph->GetNodesOfClass(VariableGets);
		for (UK2Node_VariableGet* VariableGet : VariableGets)
		{
			if (VariableGet && VariableGet->VariableReference.GetMemberName() == FName(TEXT("CharacterMovement"))
				&& VariableGet->FindPin(TEXT("self"), EGPD_Input))
			{
				MovementGetter = VariableGet;
				break;
			}
		}

		UK2Node_VariableSet* MovementSetter = nullptr;
		TArray<UK2Node_VariableSet*> VariableSets;
		EventGraph->GetNodesOfClass(VariableSets);
		for (UK2Node_VariableSet* VariableSet : VariableSets)
		{
			const FName MemberName = VariableSet ? VariableSet->VariableReference.GetMemberName() : NAME_None;
			if (VariableSet && (MemberName == FName(TEXT("CharacterMovement"))
				|| MemberName == FName(TEXT("Character Movement"))))
			{
				MovementSetter = VariableSet;
				break;
			}
		}

		if (!InitializeEvent || !TryGetPawnOwner || !MovementGetter || !MovementSetter)
		{
			OutError = TEXT("Enemy movement initialization nodes are missing");
			return false;
		}

		UK2Node_DynamicCast* CharacterCast = nullptr;
		TArray<UK2Node_DynamicCast*> CastNodes;
		EventGraph->GetNodesOfClass(CastNodes);
		for (UK2Node_DynamicCast* CastNode : CastNodes)
		{
			if (CastNode && CastNode->TargetType == ACharacter::StaticClass())
			{
				CharacterCast = CastNode;
				break;
			}
		}

		if (!CharacterCast)
		{
			FGraphNodeCreator<UK2Node_DynamicCast> CastCreator(*EventGraph);
			CharacterCast = CastCreator.CreateNode();
			CharacterCast->TargetType = ACharacter::StaticClass();
			CastCreator.Finalize();
			CharacterCast->NodePosX = 320;
			CharacterCast->NodePosY = -760;
		}

		const UEdGraphSchema* Schema = EventGraph->GetSchema();
		UEdGraphPin* InitializeThen = InitializeEvent->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* CastExecute = CharacterCast->FindPin(TEXT("execute"), EGPD_Input);
		UEdGraphPin* PawnOwner = TryGetPawnOwner->FindPin(TEXT("ReturnValue"), EGPD_Output);
		UEdGraphPin* CastObject = CharacterCast->FindPin(TEXT("Object"), EGPD_Input);
		UEdGraphPin* CastThen = CharacterCast->FindPin(TEXT("then"), EGPD_Output);
		const FString CastCharacterPinName = FString(UEdGraphSchema_K2::PN_CastedValuePrefix)
			+ ACharacter::StaticClass()->GetDisplayNameText().ToString();
		UEdGraphPin* CastCharacter = CharacterCast->FindPin(*CastCharacterPinName, EGPD_Output);
		UEdGraphPin* MovementSelf = MovementGetter->FindPin(TEXT("self"), EGPD_Input);
		UEdGraphPin* MovementExecute = MovementSetter->FindPin(TEXT("execute"), EGPD_Input);
		if (!Schema || !InitializeThen || !CastExecute || !PawnOwner || !CastObject || !CastCharacter || !MovementSelf
			|| !CastThen || !MovementExecute
			|| !Schema->TryCreateConnection(InitializeThen, CastExecute)
			|| !Schema->TryCreateConnection(PawnOwner, CastObject))
		{
			OutError = TEXT("Failed to wire generic character movement cast");
			return false;
		}

		MovementSelf->BreakAllPinLinks();
		if (!Schema->TryCreateConnection(CastCharacter, MovementSelf))
		{
			OutError = TEXT("Failed to connect CharacterMovement to generic character cast");
			return false;
		}

		MovementExecute->BreakAllPinLinks();
		if (!Schema->TryCreateConnection(CastThen, MovementExecute))
		{
			OutError = TEXT("Failed to route generic character cast into CharacterMovement setter");
			return false;
		}

		UK2Node_VariableSet* GroundSpeedSetter = nullptr;
		for (UK2Node_VariableSet* VariableSet : VariableSets)
		{
			if (VariableSet && VariableSet->VariableReference.GetMemberName() == FName(TEXT("GroundSpeed")))
			{
				GroundSpeedSetter = VariableSet;
				break;
			}
		}
		if (!GroundSpeedSetter)
		{
			OutError = TEXT("GroundSpeed setter is missing from the enemy update graph");
			return false;
		}

		UK2Node_CallFunction* DiagnosticCall = nullptr;
		const UFunction* DiagnosticFunction = UAuraAnimationDiagnosticsLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UAuraAnimationDiagnosticsLibrary, LogCivilianAnimationState));
		if (!DiagnosticFunction)
		{
			OutError = TEXT("Civilian animation diagnostic function is missing");
			return false;
		}
		TArray<UK2Node_CallFunction*> DiagnosticCandidates;
		EventGraph->GetNodesOfClass(DiagnosticCandidates);
		for (UK2Node_CallFunction* Candidate : DiagnosticCandidates)
		{
			if (Candidate && Candidate->GetTargetFunction() == DiagnosticFunction)
			{
				DiagnosticCall = Candidate;
				break;
			}
		}
		if (!DiagnosticCall)
		{
			FEdGraphSchemaAction_K2NewNode Action;
			UK2Node_CallFunction* Template = NewObject<UK2Node_CallFunction>(EventGraph);
			Template->SetFromFunction(DiagnosticFunction);
			Action.NodeTemplate = Template;
			DiagnosticCall = Cast<UK2Node_CallFunction>(Action.PerformAction(EventGraph, nullptr, FVector2D(1280.f, 16.f), true));
		}
		if (!DiagnosticCall)
		{
			OutError = TEXT("Failed to create Civilian animation diagnostic call");
			return false;
		}

		FEdGraphSchemaAction_K2NewNode GroundSpeedAction;
		UK2Node_VariableGet* GroundSpeedTemplate = NewObject<UK2Node_VariableGet>(EventGraph);
		GroundSpeedTemplate->VariableReference.SetSelfMember(FName(TEXT("GroundSpeed")));
		GroundSpeedAction.NodeTemplate = GroundSpeedTemplate;
		UK2Node_VariableGet* GroundSpeedGetter = nullptr;
		for (UK2Node_VariableGet* VariableGet : VariableGets)
		{
			if (VariableGet && VariableGet->VariableReference.GetMemberName() == FName(TEXT("GroundSpeed")))
			{
				GroundSpeedGetter = VariableGet;
				break;
			}
		}
		if (!GroundSpeedGetter)
		{
			GroundSpeedGetter = Cast<UK2Node_VariableGet>(
				GroundSpeedAction.PerformAction(EventGraph, nullptr, FVector2D(1120.f, 120.f), true));
		}
		if (!GroundSpeedGetter)
		{
			OutError = TEXT("Failed to create GroundSpeed diagnostic getter");
			return false;
		}

		UEdGraphPin* DiagnosticExecute = DiagnosticCall->FindPin(TEXT("execute"), EGPD_Input);
		UEdGraphPin* GroundSpeedThen = GroundSpeedSetter->FindPin(TEXT("then"), EGPD_Output);
		UEdGraphPin* DiagnosticPawn = DiagnosticCall->FindPin(TEXT("PawnOwner"), EGPD_Input);
		UEdGraphPin* DiagnosticMovement = DiagnosticCall->FindPin(TEXT("CachedMovement"), EGPD_Input);
		UEdGraphPin* DiagnosticGroundSpeed = DiagnosticCall->FindPin(TEXT("BlueprintGroundSpeed"), EGPD_Input);
		UEdGraphPin* GroundSpeedValue = GroundSpeedGetter->FindPin(TEXT("GroundSpeed"), EGPD_Output);
		UEdGraphPin* MovementValue = MovementGetter->FindPin(TEXT("CharacterMovement"), EGPD_Output);
		if (!DiagnosticExecute || !GroundSpeedThen || !DiagnosticPawn || !DiagnosticMovement || !DiagnosticGroundSpeed
			|| !GroundSpeedValue || !MovementValue)
		{
			OutError = TEXT("Civilian animation diagnostic pins are missing");
			return false;
		}

		DiagnosticExecute->BreakAllPinLinks();
		if (!Schema->TryCreateConnection(GroundSpeedThen, DiagnosticExecute)
			|| !Schema->TryCreateConnection(PawnOwner, DiagnosticPawn)
			|| !Schema->TryCreateConnection(MovementValue, DiagnosticMovement)
			|| !Schema->TryCreateConnection(GroundSpeedValue, DiagnosticGroundSpeed))
		{
			OutError = TEXT("Failed to wire Civilian animation diagnostics");
			return false;
		}

		EventGraph->NotifyGraphChanged();
		return true;
	}

	void WriteReport(bool bConfigured, bool bEventFound, bool bParentCallFound, bool bEventEnabled, bool bParentCallEnabled, const FString& Error, bool bEnemyMovementConfigured = false)
	{
		const FString AbsolutePath = FPaths::ProjectDir() / ReportPath;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		const FString Json = FString::Printf(
			TEXT("{\n  \"configured\": %s,\n  \"updateEventFound\": %s,\n  \"parentCallFound\": %s,\n  \"updateEventEnabled\": %s,\n  \"parentCallEnabled\": %s,\n  \"enemyMovementConfigured\": %s,\n  \"error\": \"%s\"\n}\n"),
			bConfigured ? TEXT("true") : TEXT("false"),
			bEventFound ? TEXT("true") : TEXT("false"),
			bParentCallFound ? TEXT("true") : TEXT("false"),
			bEventEnabled ? TEXT("true") : TEXT("false"),
			bParentCallEnabled ? TEXT("true") : TEXT("false"),
			bEnemyMovementConfigured ? TEXT("true") : TEXT("false"),
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
	UAnimBlueprint* EnemyBlueprint = LoadObject<UAnimBlueprint>(nullptr, EnemyBlueprintPath);
	FString EnemyError;
	if (!ConfigureEnemyMovementOwner(EnemyBlueprint, EnemyError))
	{
		WriteReport(false, false, false, false, false, EnemyError);
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] %s"), *EnemyError);
		return 1;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(EnemyBlueprint);
	FKismetEditorUtilities::CompileBlueprint(EnemyBlueprint);
	if (EnemyBlueprint->Status == BS_Error || !SaveBlueprint(EnemyBlueprint))
	{
		WriteReport(false, false, false, false, false, TEXT("Failed to compile or save generic enemy movement path"));
		UE_LOG(LogTemp, Error, TEXT("[CivilianAnimBlueprint] Failed to compile or save %s"), EnemyBlueprintPath);
		return 1;
	}

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

	WriteReport(true, true, true, true, true, TEXT(""), true);
	UE_LOG(LogTemp, Display, TEXT("[CivilianAnimBlueprint] PASS Asset=%s BlueprintUpdateAnimation=enabled ParentCall=enabled"), BlueprintPath);
	return 0;
}
