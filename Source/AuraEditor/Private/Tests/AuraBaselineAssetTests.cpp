#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Animation/AnimMontage.h"
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

#endif
