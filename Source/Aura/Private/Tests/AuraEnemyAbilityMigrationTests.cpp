#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "AuraAbilityGraph/Public/Nodes/AbilityActionNode.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraEnemyAbilityDefinitionsTest,
	"Aura.Abilities.Enemy.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraEnemyAbilityDefinitionsTest::RunTest(const FString& Parameters)
{
	struct FExpectedDefinition
	{
		const TCHAR* File;
		const TCHAR* Tag;
		const TCHAR* FinalNode;
		const TCHAR* CurveRow;
	};
	const FExpectedDefinition Expected[] = {
		{TEXT("EnemyFireBolt.xml"), TEXT("Abilities.FireBolt"), TEXT("SpawnProjectile"), TEXT("FireBolt")},
		{TEXT("EnemyRangedAttack.xml"), TEXT("Abilities.Ranged"), TEXT("SpawnProjectile"), TEXT("Ranged")},
		{TEXT("EnemyMeleeAttack.xml"), TEXT("Abilities.Melee"), TEXT("EnemyMeleeDamage"), TEXT("Melee")},
		{TEXT("EnemyHitReact.xml"), TEXT("Effects.HitReact"), TEXT("EnemyHitReact"), TEXT("")},
	};

	for (const FExpectedDefinition& Item : Expected)
	{
		UAuraAbilityDefinition* Definition = UAuraAbilitySystemLibrary::LoadAbilityDefinitionFromXMLFile(
			FString::Printf(TEXT("/Game/AbilityDefinitions/%s"), Item.File));
		if (!TestNotNull(Item.File, Definition)) continue;
		TestEqual(TEXT("Primary ability tag"), Definition->AbilityTag.ToString(), FString(Item.Tag));
		TestTrue(TEXT("Primary tag is included in activation tags"), Definition->AbilityTags.HasTagExact(Definition->AbilityTag));
		TestNotNull(TEXT("Root graph node"), Definition->RootNode.Get());

		UAuraAbilityActionNode* FinalNode = Definition->RootNode;
		if (Definition->RootNode && !Definition->RootNode->Children.IsEmpty())
		{
			FinalNode = Definition->RootNode->Children.Last();
		}
		if (FinalNode)
		{
			TestEqual(TEXT("Final graph node"), FinalNode->NodeClassName, FString(Item.FinalNode));
		}
		if (FCString::Strlen(Item.CurveRow) > 0)
		{
			TestEqual(TEXT("Legacy damage curve row"), Definition->Damage.Curve.RowName, FName(Item.CurveRow));
			TestNotNull(TEXT("Legacy damage curve table"), Definition->Damage.Curve.CurveTable.Get());
			TestTrue(TEXT("Attack can be activated through parent AI tag"),
				Definition->AbilityTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Abilities.Attack"))));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraEnemyAbilityConfigTest,
	"Aura.Abilities.Enemy.Config",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraEnemyAbilityConfigTest::RunTest(const FString& Parameters)
{
	FString Json;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/EnemyAbilityConfig.json"));
	if (!TestTrue(TEXT("Enemy config is readable"), FFileHelper::LoadFileToString(Json, *Path))) return false;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!TestTrue(TEXT("Enemy config is valid JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())) return false;

	const TArray<TSharedPtr<FJsonValue>>* Common = nullptr;
	const TSharedPtr<FJsonObject>* Classes = nullptr;
	TestTrue(TEXT("Common definitions exist"), Root->TryGetArrayField(TEXT("commonAbilityDefinitions"), Common) && Common && Common->Num() == 1);
	if (!TestTrue(TEXT("Class map exists"), Root->TryGetObjectField(TEXT("classes"), Classes) && Classes)) return false;
	for (const TCHAR* ClassName : {TEXT("Elementalist"), TEXT("Warrior"), TEXT("Ranger")})
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		TestTrue(ClassName, (*Classes)->TryGetArrayField(ClassName, Values) && Values && Values->Num() == 1);
	}
	return true;
}

#endif
