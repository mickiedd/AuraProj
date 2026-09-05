#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "Animation/AuraCharacterAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Animation/AnimMontage.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "Sound/SoundCue.h"
#include "UObject/UnrealType.h"

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

#endif
