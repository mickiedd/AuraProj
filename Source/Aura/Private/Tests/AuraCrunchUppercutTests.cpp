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

#endif
