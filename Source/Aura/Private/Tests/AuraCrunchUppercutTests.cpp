// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/Crunch/AuraCrunchDash.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchGroundBlast.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchTornado.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchUppercut.h"
#include "AuraGameplayTags.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchSkillIdentityTest,
	"Aura.Migration.Crunch.Skills.IdentityAndInputContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchSkillIdentityTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const TArray<TTuple<TSubclassOf<UAuraCrunchAbilityBase>, FGameplayTag, FGameplayTag>> Skills = {
		MakeTuple(TSubclassOf<UAuraCrunchAbilityBase>(UAuraCrunchUppercut::StaticClass()), Tags.InputTag_1, Tags.Abilities_Melee_CrunchUppercut),
		MakeTuple(TSubclassOf<UAuraCrunchAbilityBase>(UAuraCrunchDash::StaticClass()), Tags.InputTag_2, Tags.Abilities_Melee_CrunchDash),
		MakeTuple(TSubclassOf<UAuraCrunchAbilityBase>(UAuraCrunchGroundBlast::StaticClass()), Tags.InputTag_3, Tags.Abilities_Melee_CrunchGroundBlast),
		MakeTuple(TSubclassOf<UAuraCrunchAbilityBase>(UAuraCrunchTornado::StaticClass()), Tags.InputTag_4, Tags.Abilities_Melee_CrunchTornado)
	};
	for (const auto& Skill : Skills)
	{
		const UAuraCrunchAbilityBase* CDO = Skill.Get<0>() ? Skill.Get<0>().GetDefaultObject() : nullptr;
		TestNotNull(TEXT("Crunch skill CDO exists"), CDO);
		if (!CDO) continue;
		AddInfo(FString::Printf(TEXT("%s startup=%s expected=%s"), *CDO->GetClass()->GetName(), *CDO->GetStartupInputTag().ToString(), *Skill.Get<1>().ToString()));
		TestEqual(TEXT("Crunch skill declares its input slot"), CDO->GetStartupInputTag(), Skill.Get<1>());
		TestTrue(TEXT("Crunch skill declares a stable ability identity"), CDO->GetAssetTags().HasTagExact(Skill.Get<2>()));
		TestTrue(TEXT("Crunch skill uses per-execution instancing"), CDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution);
		TestTrue(TEXT("Crunch skill is locally predicted"), CDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchRoleLoadoutContractTest,
	"Aura.Migration.Crunch.Skills.RoleLoadoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchRoleLoadoutContractTest::RunTest(const FString& Parameters)
{
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/RoleConfig.json"));
	FString Config;
	TestTrue(TEXT("Crunch RoleConfig is readable"), FFileHelper::LoadFileToString(Config, *ConfigPath));
	TestTrue(TEXT("Crunch owns UpperCut on InputTag.1"), Config.Contains(TEXT("/Script/Aura.AuraCrunchUppercut")));
	TestTrue(TEXT("Crunch owns Dash on InputTag.2"), Config.Contains(TEXT("/Script/Aura.AuraCrunchDash")));
	TestTrue(TEXT("Crunch owns GroundBlast on InputTag.3"), Config.Contains(TEXT("/Script/Aura.AuraCrunchGroundBlast")));
	TestTrue(TEXT("Crunch owns Tornado on InputTag.4"), Config.Contains(TEXT("/Script/Aura.AuraCrunchTornado")));
	TestTrue(TEXT("Crunch retains the LMB combo owner"), Config.Contains(TEXT("\"lmbAbility\": \"/Script/Aura.AuraMeleeAttack\"")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchGroundBlastAuthorityContractTest,
	"Aura.Migration.Crunch.GroundBlast.AuthorityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchGroundBlastAuthorityContractTest::RunTest(const FString& Parameters)
{
	const UAuraCrunchGroundBlast* CDO = UAuraCrunchGroundBlast::StaticClass()->GetDefaultObject<UAuraCrunchGroundBlast>();
	if (!TestNotNull(TEXT("GroundBlast CDO exists"), CDO))
	{
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestEqual(TEXT("GroundBlast remains bound to Num.3"), CDO->GetStartupInputTag(), Tags.InputTag_3);
	TestTrue(TEXT("GroundBlast keeps its stable ability identity"),
		CDO->GetAssetTags().HasTagExact(Tags.Abilities_Melee_CrunchGroundBlast));

	const FFloatProperty* RadiusProperty = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("TargetAreaRadius"));
	const FFloatProperty* RangeProperty = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("TargetTraceRange"));
	const FFloatProperty* HeightProperty = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("MaxTargetHeight"));
	const FFloatProperty* PushProperty = FindFProperty<FFloatProperty>(CDO->GetClass(), TEXT("TargetPushSpeed"));
	TestNotNull(TEXT("GroundBlast radius is reflected"), RadiusProperty);
	TestNotNull(TEXT("GroundBlast range is reflected"), RangeProperty);
	TestNotNull(TEXT("GroundBlast height bound is reflected"), HeightProperty);
	TestNotNull(TEXT("GroundBlast push speed is reflected"), PushProperty);
	if (RadiusProperty && RangeProperty && HeightProperty && PushProperty)
	{
		TestEqual(TEXT("GroundBlast area radius is frozen"), RadiusProperty->GetPropertyValue_InContainer(CDO), 300.f);
		TestEqual(TEXT("GroundBlast center range is frozen"), RangeProperty->GetPropertyValue_InContainer(CDO), 2000.f);
		TestEqual(TEXT("GroundBlast height bound is frozen"), HeightProperty->GetPropertyValue_InContainer(CDO), 250.f);
		TestEqual(TEXT("GroundBlast push speed is frozen"), PushProperty->GetPropertyValue_InContainer(CDO), 3000.f);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchNumberedInputPressContractTest,
	"Aura.Migration.Crunch.NumberedInput.PressEdgeActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchNumberedInputPressContractTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Input test world"), World)) return false;
	World->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	AAuraRoleApplicationTestActor* Owner = World->SpawnActor<AAuraRoleApplicationTestActor>();
	if (TestNotNull(TEXT("Input test owner"), Owner))
	{
		Owner->InitializeTestAbilityActorInfo();
		UAuraAbilitySystemComponent* ASC = Owner->GetTestASC();
		const auto& Tags = FAuraGameplayTags::Get();
		for (const FGameplayTag Slot : {Tags.InputTag_1, Tags.InputTag_2, Tags.InputTag_3, Tags.InputTag_4})
		{
			FGameplayAbilitySpec Spec(UAuraGameplayAbility::StaticClass(), 1);
			Spec.GetDynamicSpecSourceTags().AddTag(Slot);
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			ASC->AbilityInputTagPressed(Slot);
			TestTrue(TEXT("Press activates numbered skill"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
			ASC->CancelAbilityHandle(Handle);
			for (int32 Frame = 0; Frame < 30; ++Frame) ASC->AbilityInputTagHeld(Slot);
			TestFalse(TEXT("Held frames cannot restart completed skill"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
			ASC->AbilityInputTagReleased(Slot);
			ASC->AbilityInputTagPressed(Slot);
			TestTrue(TEXT("Next press activates again"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
			ASC->ClearAbility(Handle);
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraGroundBlastPresentationTest,
	"Aura.Migration.Crunch.GroundBlast.PresentationAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGroundBlastPresentationTest::RunTest(const FString& Parameters)
{
	const UAuraCrunchGroundBlast* CDO = GetDefault<UAuraCrunchGroundBlast>();
	const FObjectProperty* Property = FindFProperty<FObjectProperty>(CDO->GetClass(), TEXT("CastMontage"));
	UAnimMontage* Montage = Property ? Cast<UAnimMontage>(Property->GetObjectPropertyValue_InContainer(CDO)) : nullptr;
	if (TestNotNull(TEXT("GroundBlast cast montage loads"), Montage))
	{
		TestTrue(TEXT("Cast has playable length"), Montage->GetPlayLength() > 0.f);
		TestEqual(TEXT("Dedicated cast contains one slot"), Montage->SlotAnimTracks.Num(), 1);
		for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
		{
			TestEqual(TEXT("Cast uses output slot"), Slot.SlotName, FName(TEXT("DefaultSlot")));
			TestEqual(TEXT("Cast has a segment"), Slot.AnimTrack.AnimSegments.Num(), 1);
			for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				TestNotNull(TEXT("Cast animation dependency resolves"), Segment.GetAnimReference().Get());
				if (Segment.GetAnimReference()) TestEqual(TEXT("Cast skeleton matches segment"),
					Montage->GetSkeleton(), Segment.GetAnimReference()->GetSkeleton());
			}
		}
	}
	const AAuraCharacterBase* Character = GetDefault<AAuraCharacterBase>();
	TestTrue(TEXT("Production character routes gameplay cues"), Character->Implements<UGameplayCueInterface>());
	TestNotNull(TEXT("Native GroundBlast cue handler exists"), Character->FindFunction(TEXT("GameplayCue_Crunch_GroundBlast")));
	TestNotNull(TEXT("GroundBlast Niagara system loads"), Character->GroundBlastEffect.Get());
	return !HasAnyErrors();
}

#endif
