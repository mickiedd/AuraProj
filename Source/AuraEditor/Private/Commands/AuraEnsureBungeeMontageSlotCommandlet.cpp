// Copyright Druid Mechanics

#include "Commands/AuraEnsureBungeeMontageSlotCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachine.h"
#include "EdGraph/EdGraph.h"
#include "GameplayTagContainer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace AuraEnsureBungeeMontageSlotPrivate
{
	constexpr TCHAR BlueprintPath[] = TEXT("/Game/BungeeMan/Blueprints/ABP_Bungee.ABP_Bungee");
	constexpr TCHAR MontagePath[] = TEXT("/Game/BungeeMan/Animations/AM_BungeeMan_FireGun.AM_BungeeMan_FireGun");
	constexpr TCHAR MontageEventNotifyClassPath[] = TEXT("/Game/Blueprints/AnimNotifies/AN_MontageEvent.AN_MontageEvent_C");
	const FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.FireGun"), false);

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

	bool LinkPins(const UEdGraphSchema* Schema, UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		return Schema && OutputPin && InputPin && Schema->TryCreateConnection(OutputPin, InputPin);
	}

	bool SavePackageForAsset(UObject* Asset)
	{
		if (!Asset || !Asset->GetPackage()) return false;
		Asset->GetPackage()->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs);
	}

	bool EnsureFireGunNotify(UAnimMontage* Montage, bool& bOutChanged)
	{
		if (!Montage) return false;
		for (int32 Index = Montage->Notifies.Num() - 1; Index >= 0; --Index)
		{
			FAnimNotifyEvent& Event = Montage->Notifies[Index];
			const UObject* NotifyObject = Event.Notify
				? static_cast<const UObject*>(Event.Notify)
				: static_cast<const UObject*>(Event.NotifyStateClass);
			if (NotifyObject && NotifyObject->IsA(UAnimNotify::StaticClass())
				&& NotifyObject->GetClass()->GetName().Contains(TEXT("AnimNotify_PlayMontageNotify")))
			{
				Montage->Notifies.RemoveAt(Index);
				bOutChanged = true;
			}
		}

		for (FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const UObject* NotifyObject = Event.Notify
				? static_cast<const UObject*>(Event.Notify)
				: static_cast<const UObject*>(Event.NotifyStateClass);
			if (!NotifyObject) continue;
			const FStructProperty* EventTagProperty = CastField<FStructProperty>(
				NotifyObject->GetClass()->FindPropertyByName(TEXT("EventTag")));
			if (!EventTagProperty || EventTagProperty->Struct != FGameplayTag::StaticStruct()) continue;
			const FGameplayTag* AuthoredTag = EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(NotifyObject);
			if (AuthoredTag && *AuthoredTag == FireEventTag)
			{
				return true;
			}
		}

		UClass* NotifyClass = LoadClass<UAnimNotify>(nullptr, MontageEventNotifyClassPath);
		if (!NotifyClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Montage event notify class failed to load: %s"), MontageEventNotifyClassPath);
			return false;
		}
		UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass, NAME_None, RF_Transactional);
		if (!Notify) return false;
		if (FStructProperty* EventTagProperty = FindFProperty<FStructProperty>(Notify->GetClass(), TEXT("EventTag")))
		{
			if (EventTagProperty->Struct != FGameplayTag::StaticStruct()) return false;
			FGameplayTag* AuthoredTag = EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Notify);
			if (!AuthoredTag) return false;
			*AuthoredTag = FireEventTag;
		}
		else
		{
			return false;
		}

		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.MontageTickType = EMontageNotifyTickType::BranchingPoint;
		Event.Link(Montage, FMath::Clamp(Montage->GetPlayLength() * 0.5f, 0.01f, FMath::Max(0.01f, Montage->GetPlayLength() - 0.01f)), 0);
		Montage->Notifies.Add(MoveTemp(Event));
		Montage->SortNotifies();
		Montage->RefreshCacheData();
		bOutChanged = true;
		return true;
	}
}

int32 UAuraEnsureBungeeMontageSlotCommandlet::Main(const FString& Params)
{
	using namespace AuraEnsureBungeeMontageSlotPrivate;
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, MontagePath);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] FireGun montage failed to load: %s"), MontagePath);
		return 1;
	}
	bool bMontageChanged = false;
	if (!EnsureFireGunNotify(Montage, bMontageChanged))
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Failed to ensure FireGun branching-point notify on %s."), MontagePath);
		return 1;
	}
	if (bMontageChanged && !SavePackageForAsset(Montage))
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Failed to save FireGun montage: %s"), MontagePath);
		return 1;
	}
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] AnimBlueprint failed to load: %s"), BlueprintPath);
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
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] AnimGraph failed to load."));
		return 1;
	}

	TArray<UAnimGraphNode_Base*> RootNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Root::StaticClass(), RootNodes);
	TArray<UAnimGraphNode_Base*> StateMachineNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_StateMachine::StaticClass(), StateMachineNodes);
	if (RootNodes.Num() != 1 || StateMachineNodes.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Expected one root and one state machine, found Root=%d StateMachine=%d."),
			RootNodes.Num(), StateMachineNodes.Num());
		return 1;
	}

	TArray<UAnimGraphNode_Base*> SlotNodes;
	AnimationGraph->GetGraphNodesOfClass(UAnimGraphNode_Slot::StaticClass(), SlotNodes);
	if (SlotNodes.Num() > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Expected at most one montage slot, found %d."), SlotNodes.Num());
		return 1;
	}

	UAnimGraphNode_Slot* SlotNode = SlotNodes.Num() == 1
		? CastChecked<UAnimGraphNode_Slot>(SlotNodes[0])
		: nullptr;
	if (!SlotNode)
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> SlotCreator(*AnimationGraph);
		SlotNode = SlotCreator.CreateNode();
		SlotNode->NodePosX = -150;
		SlotNode->NodePosY = 0;
		SlotCreator.Finalize();
	}
	SlotNode->Node.SlotName = FName(TEXT("DefaultSlot"));
	SlotNode->Node.bAlwaysUpdateSourcePose = true;

	UAnimGraphNode_Root* RootNode = CastChecked<UAnimGraphNode_Root>(RootNodes[0]);
	UAnimGraphNode_StateMachine* StateMachineNode = CastChecked<UAnimGraphNode_StateMachine>(StateMachineNodes[0]);
	UEdGraphPin* StateMachinePose = StateMachineNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* SlotSource = SlotNode->FindPin(TEXT("Source"), EGPD_Input);
	UEdGraphPin* SlotPose = SlotNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* RootResult = RootNode->FindPin(TEXT("Result"), EGPD_Input);
	if (!StateMachinePose || !SlotSource || !SlotPose || !RootResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Required pose pins were not found."));
		return 1;
	}

	SlotSource->BreakAllPinLinks();
	RootResult->BreakAllPinLinks();
	const bool bLinked = LinkPins(AnimationGraph->GetSchema(), StateMachinePose, SlotSource)
		&& LinkPins(AnimationGraph->GetSchema(), SlotPose, RootResult);
	if (!bLinked)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Failed to wire StateMachine -> DefaultSlot -> Output."));
		return 1;
	}

	AnimationGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Configured AnimBlueprint failed to compile."));
		return 1;
	}
	if (!SaveBlueprint(Blueprint))
	{
		UE_LOG(LogTemp, Error, TEXT("[BungeeMontageSlot] Failed to save configured AnimBlueprint."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[BungeeMontageSlot] PASS Asset=%s Slot=DefaultSlot Links=StateMachine->Slot->Output Montage=%s Notify=Event.Montage.FireGun"), BlueprintPath, MontagePath);
	return 0;
}
