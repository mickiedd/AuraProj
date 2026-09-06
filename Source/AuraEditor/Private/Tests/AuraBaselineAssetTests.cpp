#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "Animation/AuraCharacterAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachine.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CallFunction.h"
#include "Animation/AuraAnimationDiagnosticsLibrary.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "GameFramework/Character.h"
#include "Sound/SoundCue.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraBaselineSoundCueGuidTest,
	"Aura.RoleBattle.Day41.Assets.SoundCueGraphGuids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraBaselineSoundCueGuidTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/MilitaryWeapDark/Sound/Rifle/Rifle_ImpactSurface_Cue.Rifle_ImpactSurface_Cue"),
		TEXT("/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue")
	};
	for (const TCHAR* Path : Paths)
	{
		USoundCue* Cue = LoadObject<USoundCue>(nullptr, Path);
		if (!TestNotNull(Path, Cue)) continue;
		UEdGraph* Graph = Cue->SoundCueGraph;
		if (!TestNotNull(TEXT("Persisted sound graph"), Graph)) continue;
		TestTrue(TEXT("Sound graph is nonempty"), !Graph->Nodes.IsEmpty());
		TestNotNull(TEXT("Playable sound root retained"), Cue->FirstNode.Get());
		TSet<FGuid> Guids;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!TestNotNull(TEXT("Persisted graph node"), Node)) continue;
			TestTrue(TEXT("Graph node has a valid persisted GUID"), Node->NodeGuid.IsValid());
			TestFalse(TEXT("Graph node GUID is unique within its cue"), Guids.Contains(Node->NodeGuid));
			Guids.Add(Node->NodeGuid);
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchComboNativeContractTest,
	"Aura.Migration.CrunchCombo.NativeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchComboNativeContractTest::RunTest(const FString& Parameters)
{
	const TCHAR* AbilityPath = TEXT("/Script/Aura.AuraMeleeAttack");
	UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, AbilityPath);
	if (!TestNotNull(TEXT("Native Crunch combo ability class loads"), AbilityClass)) return false;
	TestTrue(TEXT("Native class derives from UAuraMeleeAttack"), AbilityClass->IsChildOf(UAuraMeleeAttack::StaticClass()));

	const UGameplayAbility* DefaultAbility = AbilityClass->GetDefaultObject<UGameplayAbility>();
	const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Melee.CrunchCombo"));
	TestTrue(TEXT("Native class exposes the Crunch combo asset tag"), DefaultAbility->GetAssetTags().HasTagExact(ComboTag));

	const FObjectPropertyBase* MontageProperty = FindFProperty<FObjectPropertyBase>(AbilityClass, TEXT("ComboMontage"));
	if (!TestNotNull(TEXT("Native class has a ComboMontage property"), MontageProperty)) return false;
	const UObject* MontageObject = MontageProperty->GetObjectPropertyValue_InContainer(DefaultAbility);
	const UAnimMontage* Montage = Cast<UAnimMontage>(MontageObject);
	if (!TestNotNull(TEXT("Native class CDO resolves the generated montage"), Montage)) return false;
	TestEqual(TEXT("Native CDO points at the target-owned Crunch montage"), Montage->GetPathName(),
		FString(TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchComboV4.AM_CrunchComboV4")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchLocomotionAnimBlueprintTest,
	"Aura.Migration.Crunch.Presentation.LocomotionGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchLocomotionAnimBlueprintTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV5.ABP_Crunch_AuraV5"));
	if (!TestNotNull(TEXT("Crunch locomotion AnimBlueprint loads"), Blueprint)) return false;
	TestTrue(TEXT("Crunch AnimBlueprint uses Aura's movement-state AnimInstance"),
		Blueprint->ParentClass.Get() == UAuraCharacterAnimInstance::StaticClass());

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}
	if (!TestNotNull(TEXT("Crunch AnimGraph exists"), AnimGraph)) return false;

	TArray<UAnimGraphNode_Base*> SequenceNodes;
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_SequencePlayer::StaticClass(), SequenceNodes);
	TestEqual(TEXT("Idle and jog are the two locomotion sequence players"), SequenceNodes.Num(), 2);
	TSet<FString> SequencePaths;
	for (UAnimGraphNode_Base* Node : SequenceNodes)
	{
		if (const UAnimGraphNode_SequencePlayer* Player = Cast<UAnimGraphNode_SequencePlayer>(Node))
		{
			if (const UAnimSequenceBase* Sequence = Player->Node.GetSequence()) SequencePaths.Add(Sequence->GetPathName());
		}
	}
	TestTrue(TEXT("Target-owned combat idle feeds the locomotion graph"), SequencePaths.Contains(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Idle_CombatV4.Idle_CombatV4")));
	TestTrue(TEXT("Target-owned forward jog feeds the locomotion graph"), SequencePaths.Contains(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Jog_FwdV4.Jog_FwdV4")));

	TArray<UAnimGraphNode_Base*> BlendNodes;
	TArray<UAnimGraphNode_Base*> SlotNodes;
	TArray<UAnimGraphNode_Base*> RootNodes;
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_BlendListByBool::StaticClass(), BlendNodes);
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_Slot::StaticClass(), SlotNodes);
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootNodes);
	TestEqual(TEXT("One movement selector exists"), BlendNodes.Num(), 1);
	TestEqual(TEXT("One montage slot exists"), SlotNodes.Num(), 1);
	TestEqual(TEXT("One animation output exists"), RootNodes.Num(), 1);
	if (BlendNodes.Num() != 1 || SlotNodes.Num() != 1 || RootNodes.Num() != 1) return false;

	const UEdGraphPin* ActiveValue = BlendNodes[0]->FindPin(TEXT("bActiveValue"), EGPD_Input);
	const UEdGraphPin* TruePose = BlendNodes[0]->FindPin(TEXT("BlendPose_0"), EGPD_Input);
	const UEdGraphPin* FalsePose = BlendNodes[0]->FindPin(TEXT("BlendPose_1"), EGPD_Input);
	const UEdGraphPin* SlotSource = SlotNodes[0]->FindPin(TEXT("Source"), EGPD_Input);
	const UEdGraphPin* RootResult = RootNodes[0]->FindPin(TEXT("Result"), EGPD_Input);
	auto GetLinkedSequencePath = [](const UEdGraphPin* PosePin) -> FString
	{
		if (!PosePin || PosePin->LinkedTo.Num() != 1 || !PosePin->LinkedTo[0]) return FString();
		const UAnimGraphNode_SequencePlayer* Player =
			Cast<UAnimGraphNode_SequencePlayer>(PosePin->LinkedTo[0]->GetOwningNode());
		const UAnimSequenceBase* Sequence = Player ? Player->Node.GetSequence() : nullptr;
		return Sequence ? Sequence->GetPathName() : FString();
	};
	TestTrue(TEXT("bShouldMove drives the idle/jog selector"), ActiveValue && ActiveValue->LinkedTo.Num() == 1
		&& ActiveValue->LinkedTo[0] && ActiveValue->LinkedTo[0]->PinName == TEXT("bShouldMove"));
	TestEqual(TEXT("Moving selects the forward jog pose"), GetLinkedSequencePath(TruePose),
		FString(TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Jog_FwdV4.Jog_FwdV4")));
	TestEqual(TEXT("Not moving selects the combat idle pose"), GetLinkedSequencePath(FalsePose),
		FString(TEXT("/Game/Assets/Characters/Crunch/Animations/Locomotion/Idle_CombatV4.Idle_CombatV4")));
	TestTrue(TEXT("Locomotion selector is the montage base pose"), SlotSource && SlotSource->LinkedTo.Num() == 1
		&& SlotSource->LinkedTo[0] && SlotSource->LinkedTo[0]->GetOwningNode()->IsA<UAnimGraphNode_BlendListByBool>());
	TestTrue(TEXT("DefaultSlot feeds the final animation output"), RootResult && RootResult->LinkedTo.Num() == 1
		&& RootResult->LinkedTo[0] && RootResult->LinkedTo[0]->GetOwningNode()->IsA<UAnimGraphNode_Slot>());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraBungeeManGunPresentationAssetTest,
	"Aura.RoleBattle.BungeeMan.Presentation.LmbMontageGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraBungeeManGunPresentationAssetTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/BungeeMan/Blueprints/ABP_Bungee.ABP_Bungee"));
	if (!TestNotNull(TEXT("BungeeMan AnimBlueprint loads"), Blueprint)) return false;

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/BungeeMan/Animations/AM_BungeeMan_FireGun.AM_BungeeMan_FireGun"));
	USkeleton* BungeeSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/BungeeMan/SK_BungeeMan.SK_BungeeMan"));
	if (!TestNotNull(TEXT("BungeeMan LMB fire montage loads"), Montage)
		|| !TestNotNull(TEXT("BungeeMan skeleton loads"), BungeeSkeleton)) return false;
	TestEqual(TEXT("BungeeMan LMB fire montage uses the BungeeMan skeleton"), Montage->GetSkeleton(), BungeeSkeleton);
	TestTrue(TEXT("BungeeMan LMB fire montage has a playable length"), Montage->GetPlayLength() > 0.0f);
	bool bHasFireGunNotify = false;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UObject* NotifyObject = NotifyEvent.Notify
			? static_cast<const UObject*>(NotifyEvent.Notify)
			: static_cast<const UObject*>(NotifyEvent.NotifyStateClass);
		if (!NotifyObject) continue;
		const FStructProperty* EventTagProperty = CastField<FStructProperty>(
			NotifyObject->GetClass()->FindPropertyByName(TEXT("EventTag")));
		if (!EventTagProperty || EventTagProperty->Struct != FGameplayTag::StaticStruct()) continue;
		const FGameplayTag* AuthoredTag = EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(NotifyObject);
		if (AuthoredTag && *AuthoredTag == FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.FireGun"), false))
		{
			bHasFireGunNotify = true;
			break;
		}
	}
	TestTrue(TEXT("BungeeMan LMB fire montage authors the Event.Montage.FireGun gameplay notify"), bHasFireGunNotify);

	FString FireGunXml;
	const FString FireGunPath = FPaths::ProjectContentDir() / TEXT("AbilityDefinitions/FireGun.xml");
	TestTrue(TEXT("FireGun XML loads"), FFileHelper::LoadFileToString(FireGunXml, *FireGunPath));
	TestTrue(TEXT("FireGun LMB graph references the BungeeMan montage"), FireGunXml.Contains(
		TEXT("/Game/BungeeMan/Animations/AM_BungeeMan_FireGun.AM_BungeeMan_FireGun")));

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}
	if (!TestNotNull(TEXT("BungeeMan AnimGraph exists"), AnimGraph)) return false;

	TArray<UAnimGraphNode_Base*> RootNodes;
	TArray<UAnimGraphNode_Base*> StateMachineNodes;
	TArray<UAnimGraphNode_Base*> SlotNodes;
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootNodes);
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_StateMachine::StaticClass(), StateMachineNodes);
	AnimGraph->GetGraphNodesOfClass(UAnimGraphNode_Slot::StaticClass(), SlotNodes);
	TestEqual(TEXT("BungeeMan has one AnimGraph output root"), RootNodes.Num(), 1);
	TestEqual(TEXT("BungeeMan has one main state machine"), StateMachineNodes.Num(), 1);
	TestEqual(TEXT("BungeeMan has one montage slot"), SlotNodes.Num(), 1);
	if (RootNodes.Num() != 1 || StateMachineNodes.Num() != 1 || SlotNodes.Num() != 1) return false;

	const UAnimGraphNode_Slot* SlotNode = CastChecked<UAnimGraphNode_Slot>(SlotNodes[0]);
	TestEqual(TEXT("BungeeMan montage slot is DefaultSlot"), SlotNode->Node.SlotName, FName(TEXT("DefaultSlot")));
	const UEdGraphPin* RootResult = RootNodes[0]->FindPin(TEXT("Result"), EGPD_Input);
	const UEdGraphPin* SlotSource = SlotNode->FindPin(TEXT("Source"), EGPD_Input);
	const UEdGraphPin* SlotPose = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
	TestTrue(TEXT("BungeeMan slot feeds the output pose"), RootResult && RootResult->LinkedTo.Num() == 1
		&& RootResult->LinkedTo[0] && RootResult->LinkedTo[0]->GetOwningNode()->IsA<UAnimGraphNode_Slot>());
	TestTrue(TEXT("BungeeMan state machine feeds the montage slot"), SlotSource && SlotSource->LinkedTo.Num() == 1
		&& SlotSource->LinkedTo[0] && SlotSource->LinkedTo[0]->GetOwningNode()->IsA<UAnimGraphNode_StateMachine>());
	TestTrue(TEXT("BungeeMan slot pose output exists"), SlotPose != nullptr);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCivilianLocomotionAnimBlueprintTest,
	"Aura.RoleBattle.Civilian.Presentation.LocomotionGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCivilianLocomotionAnimBlueprintTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Blueprints/Character/Shaman/ABP_Shaman.ABP_Shaman"));
	if (!TestNotNull(TEXT("Civilian Shaman AnimBlueprint loads"), Blueprint)) return false;

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
	if (!TestNotNull(TEXT("Civilian AnimBlueprint EventGraph exists"), EventGraph)) return false;

	UK2Node_Event* UpdateEvent = nullptr;
	TArray<UK2Node_Event*> EventNodes;
	EventGraph->GetNodesOfClass(EventNodes);
	for (UK2Node_Event* EventNode : EventNodes)
	{
		if (EventNode && EventNode->GetFunctionName() == FName(TEXT("BlueprintUpdateAnimation")))
		{
			UpdateEvent = EventNode;
			break;
		}
	}
	TestNotNull(TEXT("Civilian BlueprintUpdateAnimation event exists"), UpdateEvent);
	TestTrue(TEXT("Civilian BlueprintUpdateAnimation event is enabled"), UpdateEvent
		&& UpdateEvent->GetDesiredEnabledState() == ENodeEnabledState::Enabled);

	UK2Node_CallParentFunction* ParentCall = nullptr;
	TArray<UK2Node_CallParentFunction*> ParentCallNodes;
	EventGraph->GetNodesOfClass(ParentCallNodes);
	for (UK2Node_CallParentFunction* ParentCallNode : ParentCallNodes)
	{
		if (ParentCallNode && ParentCallNode->GetFunctionName() == FName(TEXT("BlueprintUpdateAnimation")))
		{
			ParentCall = ParentCallNode;
			break;
		}
	}
	TestNotNull(TEXT("Civilian parent BlueprintUpdateAnimation call exists"), ParentCall);
	TestTrue(TEXT("Civilian parent BlueprintUpdateAnimation call is enabled"), ParentCall
		&& ParentCall->GetDesiredEnabledState() == ENodeEnabledState::Enabled);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCivilianMovementOwnerAnimBlueprintTest,
	"Aura.RoleBattle.Civilian.Presentation.MovementOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCivilianMovementOwnerAnimBlueprintTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Blueprints/Character/ABP_Enemy.ABP_Enemy"));
	if (!TestNotNull(TEXT("Shared enemy locomotion AnimBlueprint loads"), Blueprint)) return false;

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
	if (!TestNotNull(TEXT("Shared enemy EventGraph exists"), EventGraph)) return false;

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
	if (!TestNotNull(TEXT("Generic ACharacter movement cast exists"), CharacterCast)) return false;

	UK2Node_VariableGet* MovementGetter = nullptr;
	TArray<UK2Node_VariableGet*> VariableGets;
	EventGraph->GetNodesOfClass(VariableGets);
	for (UK2Node_VariableGet* VariableGet : VariableGets)
	{
		if (VariableGet && VariableGet->VariableReference.GetMemberName() == FName(TEXT("CharacterMovement")))
		{
			MovementGetter = VariableGet;
			break;
		}
	}
	if (!TestNotNull(TEXT("CharacterMovement getter exists"), MovementGetter)) return false;

	const UEdGraphPin* MovementSelf = MovementGetter->FindPin(TEXT("self"), EGPD_Input);
	const FString CastCharacterPinName = FString(UEdGraphSchema_K2::PN_CastedValuePrefix)
		+ ACharacter::StaticClass()->GetDisplayNameText().ToString();
	const UEdGraphPin* CastCharacter = CharacterCast->FindPin(*CastCharacterPinName, EGPD_Output);
	TestTrue(TEXT("CharacterMovement reads the generic character cast"), MovementSelf && CastCharacter
		&& MovementSelf->LinkedTo.Contains(const_cast<UEdGraphPin*>(CastCharacter)));

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
	TestNotNull(TEXT("CharacterMovement setter exists"), MovementSetter);
	const UEdGraphPin* CastThen = CharacterCast->FindPin(TEXT("then"), EGPD_Output);
	const UEdGraphPin* MovementExecute = MovementSetter ? MovementSetter->FindPin(TEXT("execute"), EGPD_Input) : nullptr;
	TestTrue(TEXT("CharacterMovement setter executes from the generic character cast"), CastThen && MovementExecute
		&& MovementExecute->LinkedTo.Contains(const_cast<UEdGraphPin*>(CastThen)));

	const UFunction* DiagnosticFunction = UAuraAnimationDiagnosticsLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAuraAnimationDiagnosticsLibrary, LogCivilianAnimationState));
	UK2Node_CallFunction* DiagnosticCall = nullptr;
	TArray<UK2Node_CallFunction*> FunctionNodes;
	EventGraph->GetNodesOfClass(FunctionNodes);
	for (UK2Node_CallFunction* FunctionNode : FunctionNodes)
	{
		if (FunctionNode && FunctionNode->GetTargetFunction() == DiagnosticFunction)
		{
			DiagnosticCall = FunctionNode;
			break;
		}
	}
	TestNotNull(TEXT("Civilian animation diagnostic call exists"), DiagnosticCall);
	const UEdGraphPin* DiagnosticExecute = DiagnosticCall ? DiagnosticCall->FindPin(TEXT("execute"), EGPD_Input) : nullptr;
	const UEdGraphPin* GroundSpeedThen = nullptr;
	for (UK2Node_VariableSet* VariableSet : VariableSets)
	{
		if (VariableSet && VariableSet->VariableReference.GetMemberName() == FName(TEXT("GroundSpeed")))
		{
			GroundSpeedThen = VariableSet->FindPin(TEXT("then"), EGPD_Output);
			break;
		}
	}
	TestTrue(TEXT("Civilian animation diagnostics run after GroundSpeed update"), DiagnosticExecute && GroundSpeedThen
		&& DiagnosticExecute->LinkedTo.Contains(const_cast<UEdGraphPin*>(GroundSpeedThen)));
	return !HasAnyErrors();
}

#endif
