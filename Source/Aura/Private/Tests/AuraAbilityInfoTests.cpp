#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Abilities/GameplayAbility.h"
#include "AuraGameplayTags.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "AuraAbilityGraph/Public/DataAbility.h"
#include "AuraAbilityGraph/Public/Nodes/Actions/SpawnShardsNode.h"
#include "AuraAbilityGraph/Public/Tests/TestDataAbility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilityInfoBoundaryTest,
	"Aura.Abilities.Metadata.RuntimeBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilityInfoBoundaryTest::RunTest(const FString& Parameters)
{
	FString Json;
	const FString JsonPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/AbilityInfo.json"));
	if (!TestTrue(TEXT("Shipped AbilityInfo.json is readable"), FFileHelper::LoadFileToString(Json, *JsonPath))) return false;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!TestTrue(TEXT("Shipped AbilityInfo.json is valid JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())) return false;

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!TestTrue(TEXT("Shipped AbilityInfo.json has an abilities array"), Root->TryGetArrayField(TEXT("abilities"), Entries) && Entries)) return false;
	TestTrue(TEXT("Shipped AbilityInfo.json contains metadata"), Entries->Num() > 0);

	const TSet<FString> AllowedFields = {
		TEXT("abilityTag"),
		TEXT("icon"),
		TEXT("backgroundMaterial"),
		TEXT("levelRequirement")
	};
	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
		if (!TestTrue(TEXT("Every ability metadata entry is an object"), Entry.IsValid())) return false;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Entry->Values)
		{
			TestTrue(
				FString::Printf(TEXT("AbilityInfo field '%s' belongs to the UI metadata boundary"), *Field.Key),
				AllowedFields.Contains(Field.Key));
		}
	}

	URuntimeAbilityInfo* RuntimeInfo = NewObject<URuntimeAbilityInfo>();
	if (!TestTrue(TEXT("Shipped ability metadata loads"), RuntimeInfo->LoadFromJSON(Json))) return false;
	const TArray<FAuraAbilityInfo> AllInfo = RuntimeInfo->GetAllAbilityInfo();
	TestEqual(TEXT("Every shipped JSON entry is retained"), AllInfo.Num(), Entries->Num());
	for (int32 Index = 1; Index < AllInfo.Num(); ++Index)
	{
		TestTrue(
			TEXT("Runtime metadata iteration is stable and tag-sorted"),
			AllInfo[Index - 1].AbilityTag.ToString() < AllInfo[Index].AbilityTag.ToString());
	}
	for (const FAuraAbilityInfo& Info : AllInfo)
	{
		TestNull(TEXT("JSON metadata does not provide an ability class"), Info.Ability.Get());
		TestFalse(TEXT("JSON metadata does not provide an ability type"), Info.AbilityType.IsValid());
		TestFalse(TEXT("JSON metadata does not provide an input slot"), Info.InputTag.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilityInfoValidationTest,
	"Aura.Abilities.Metadata.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilityInfoValidationTest::RunTest(const FString& Parameters)
{
	URuntimeAbilityInfo* RuntimeInfo = NewObject<URuntimeAbilityInfo>();
	const FString Valid = TEXT(R"json({"abilities":[{"abilityTag":"Abilities.Fire.FireBolt","levelRequirement":3}]})json");
	if (!TestTrue(TEXT("Minimal valid metadata loads"), RuntimeInfo->LoadFromJSON(Valid))) return false;
	TestEqual(TEXT("Valid level requirement is retained"), RuntimeInfo->GetAllAbilityInfo()[0].LevelRequirement, 3);

	AddExpectedError(TEXT("JSON missing 'abilities' array"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Missing abilities array is rejected"), RuntimeInfo->LoadFromJSON(TEXT("{}")));
	TestEqual(TEXT("Rejected input clears previous metadata"), RuntimeInfo->GetAllAbilityInfo().Num(), 0);

	AddExpectedError(TEXT("Ability entry missing 'abilityTag'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Entry without a stable tag is rejected"), RuntimeInfo->LoadFromJSON(TEXT(R"json({"abilities":[{}]})json")));

	AddExpectedError(TEXT("Duplicate abilityTag"), EAutomationExpectedErrorFlags::Contains, 1);
	const FString Duplicate = TEXT(R"json({"abilities":[{"abilityTag":"Abilities.Fire.FireBolt"},{"abilityTag":"Abilities.Fire.FireBolt"}]})json");
	TestFalse(TEXT("Duplicate stable tags are rejected"), RuntimeInfo->LoadFromJSON(Duplicate));
	TestEqual(TEXT("Duplicate rejection leaves no partial metadata"), RuntimeInfo->GetAllAbilityInfo().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilityRuntimeResolutionTest,
	"Aura.Abilities.Metadata.RuntimeResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilityRuntimeResolutionTest::RunTest(const FString& Parameters)
{
	const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt"));
	const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB"));
	const FGameplayTag StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Status.Unlocked"));
	if (!TestTrue(TEXT("FireBolt ability tag is registered"), AbilityTag.IsValid())) return false;
	if (!TestNotNull(TEXT("Role definitions are loaded before runtime source resolution"), UAuraAbilitySystemLibrary::GetRoleInfo(nullptr))) return false;

	UAuraAbilitySystemComponent* ASC = NewObject<UAuraAbilitySystemComponent>();
	FGameplayAbilitySpec DataSpec(UAuraDataAbility::StaticClass(), 7);
	DataSpec.GetDynamicSpecSourceTags().AddTag(AbilityTag);
	DataSpec.GetDynamicSpecSourceTags().AddTag(InputTag);
	DataSpec.GetDynamicSpecSourceTags().AddTag(StatusTag);

	const FAuraAbilityInfo Merged = ASC->GetRuntimeAbilityInfoForSpec(DataSpec);
	TestEqual(TEXT("Runtime merge preserves the stable ability tag"), Merged.AbilityTag, AbilityTag);
	TestEqual(TEXT("Runtime merge uses the live spec input tag"), Merged.InputTag, InputTag);
	TestEqual(TEXT("Runtime merge uses the live spec status tag"), Merged.StatusTag, StatusTag);
	TestTrue(TEXT("Runtime merge resolves the XML ability type"), Merged.AbilityType.IsValid());
	TestTrue(TEXT("Runtime merge resolves the XML cooldown tag"), Merged.CooldownTag.IsValid());
	TestEqual(TEXT("Runtime merge retains JSON level metadata"), Merged.LevelRequirement, 1);

	const FAuraAbilityInfo TagOnly = ASC->GetRuntimeAbilityInfoForTag(AbilityTag);
	TestEqual(TEXT("Tag lookup resolves the same ability type"), TagOnly.AbilityType, Merged.AbilityType);
	TestEqual(TEXT("Tag lookup resolves the same cooldown"), TagOnly.CooldownTag, Merged.CooldownTag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilityDefinitionCacheRecoveryTest,
	"Aura.Abilities.DefinitionCache.RecoveryAfterTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilityDefinitionCacheRecoveryTest::RunTest(const FString& Parameters)
{
	const FGameplayTag FireBoltTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt"));
	if (!TestTrue(TEXT("FireBolt ability tag is registered"), FireBoltTag.IsValid())) return false;

	// A client can lose its Login-world RoleInfo owner during travel. This mirrors the
	// replicated spec: the stable ability tag survives, but SourceObject does not.
	UAuraAbilitySystemLibrary::ClearProcessLifetimeCaches();
	if (!TestNotNull(TEXT("RoleInfo cache loaded before simulating registry loss"), UAuraAbilitySystemLibrary::GetRoleInfo(nullptr))) return false;
	UAuraAbilitySystemLibrary::ClearDefinitionRegistryForTests();
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Transient test world created"), TestWorld)) return false;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Owner = TestWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Test owner spawned in a world"), Owner))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}
	UTestDataAbility* Ability = NewObject<UTestDataAbility>(Owner);
	if (!TestNotNull(TEXT("Test data ability created"), Ability))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}
	Ability->InitTestOwner(Owner, 2, FireBoltTag);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("Fixture has the replicated tag but no SourceObject"), Ability->HasReplicatedTagOnlySpecForTest());

	const UAuraAbilityDefinition* Definition = Ability->GetDefinition();
	TestNotNull(TEXT("Definition lookup rebuilds the client cache from RoleConfig"), Definition);
	if (Definition)
	{
		TestEqual(TEXT("Recovered definition matches the replicated ability tag"), Definition->AbilityTag, FireBoltTag);
		TestNotNull(TEXT("Recovered definition has a graph root"), Definition->RootNode.Get());

		// This is the failure boundary from the log: a missing definition used to
		// charge cost, then leave RootTask null and end the ability as cancelled.
		// Activate through the recovered definition and require the first graph task
		// to remain waiting for target data.
		Ability->ActivateForTest();
		TestTrue(TEXT("Activation keeps the graph running after definition recovery"), Ability->IsGraphRunningForTest());
		TestTrue(TEXT("Activation reaches the target-data wait task"), Ability->HasPendingTargetDataForTest());
		Ability->EndForTest();
		TestFalse(TEXT("Test activation cleans up the graph after teardown"), Ability->IsGraphRunningForTest());
	}

	// Leave the process cache in the normal loaded state for subsequent automation tests.
	UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
	return Definition != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraRoleAbilityCatalogTest,
	"Aura.Abilities.Metadata.RoleCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraRoleAbilityCatalogTest::RunTest(const FString& Parameters)
{
	const URoleInfo* Roles = UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
	if (!TestNotNull(TEXT("RoleConfig.json loads for runtime resolution"), Roles)) return false;

	const FRoleDefaultInfo* AuraRole = Roles->RoleInformation.Find(TEXT("Aura"));
	if (!TestNotNull(TEXT("Aura role exists"), AuraRole)) return false;
	TestEqual(TEXT("Aura unlock catalog contains the three legacy passive sources"), AuraRole->UnlockableAbilities.Num(), 3);
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AuraRole->UnlockableAbilities)
	{
		TestNotNull(TEXT("Every unlock catalog entry resolves to a gameplay ability class"), AbilityClass.Get());
	}

	const UAuraAbilityDefinition* FireBolt = UAuraAbilitySystemLibrary::FindAbilityDefinitionByTag(
		FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt")));
	if (!TestNotNull(TEXT("Role loading registers the FireBolt XML definition"), FireBolt)) return false;
	TestTrue(TEXT("Registered FireBolt definition has an input tag"), FireBolt->InputTag.IsValid());
	TestTrue(TEXT("Registered FireBolt definition has a cooldown tag"), FireBolt->CooldownTag.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraArcaneShardsCueReplicationTest,
	"Aura.Abilities.ArcaneShards.CueReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraArcaneShardsCueReplicationTest::RunTest(const FString& Parameters)
{
	const FString AbilityPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions/ArcaneShards.xml"));
	FString XML;
	if (!TestTrue(TEXT("ArcaneShards XML is readable"), FFileHelper::LoadFileToString(XML, *AbilityPath))) return false;

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>();
	if (!TestTrue(TEXT("ArcaneShards XML parses"), Definition->LoadFromXML(XML))) return false;
	if (!TestNotNull(TEXT("ArcaneShards graph root exists"), Definition->RootNode.Get())) return false;

	const FGameplayTag CueTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.ArcaneShards"), false);
	TestTrue(TEXT("ArcaneShards gameplay cue tag is registered"), CueTag.IsValid());

	const UAuraAbilityActionNode* SpawnNode = nullptr;
	for (const UAuraAbilityActionNode* Child : Definition->RootNode->Children)
	{
		if (Child && Child->NodeClassName == TEXT("SpawnShards"))
		{
			SpawnNode = Child;
			break;
		}
	}
	if (!TestNotNull(TEXT("ArcaneShards graph contains SpawnShards"), SpawnNode)) return false;

	const USpawnShardsNode* ShardsNode = Cast<USpawnShardsNode>(SpawnNode);
	if (!TestNotNull(TEXT("ArcaneShards SpawnShards node has the expected type"), ShardsNode)) return false;
	TestEqual(TEXT("ArcaneShards SpawnShards uses the authored cue tag"),
		ShardsNode->GameplayCueTag, FString(TEXT("GameplayCue.ArcaneShards")));

	// This is deliberately a source-level network contract: the visual is produced by
	// a server-authoritative node, so the implementation must use the source ASC's
	// replicated cue API rather than a local-only cue manager call.
	const FString ImplementationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp"));
	FString Implementation;
	if (!TestTrue(TEXT("SpawnShards implementation is readable"), FFileHelper::LoadFileToString(Implementation, *ImplementationPath))) return false;
	TestTrue(TEXT("SpawnShards dispatches the cue through the source ASC"),
		Implementation.Contains(TEXT("CachedCtx.ASC->ExecuteGameplayCue(CueTag, CueParams)")));
	TestFalse(TEXT("SpawnShards does not use a local-only cue dispatch"),
		Implementation.Contains(TEXT("ExecuteGameplayCue_NonReplicated")));
	TestTrue(TEXT("SpawnShards logs the replicated cue dispatch for runtime diagnosis"),
		Implementation.Contains(TEXT("via source ASC")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilityBlueprintDecouplingTest,
	"Aura.Abilities.Metadata.BlueprintDecoupling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilityBlueprintDecouplingTest::RunTest(const FString& Parameters)
{
	const ANSICHAR LegacyReference[] = "DA_AbilityInfo";
	const TArray<FString> Packages = {
		TEXT("Blueprints/Game/BP_AuraGameMode.uasset"),
		TEXT("Blueprints/UI/WidgetController/BP_SpellMenuWidgetController.uasset"),
		TEXT("Blueprints/UI/WidgetController/BP_OverlayWidgetController.uasset"),
		TEXT("Blueprints/UI/WidgetController/BP_AttributeMenuWidgetController.uasset")
	};

	for (const FString& RelativePath : Packages)
	{
		TArray<uint8> Bytes;
		const FString FullPath = FPaths::Combine(FPaths::ProjectContentDir(), RelativePath);
		if (!TestTrue(FString::Printf(TEXT("Active Blueprint package is readable: %s"), *RelativePath), FFileHelper::LoadFileToArray(Bytes, *FullPath)))
		{
			continue;
		}

		bool bContainsLegacyReference = false;
		for (int32 Index = 0; Index + UE_ARRAY_COUNT(LegacyReference) - 1 <= Bytes.Num(); ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, LegacyReference, UE_ARRAY_COUNT(LegacyReference) - 1) == 0)
			{
				bContainsLegacyReference = true;
				break;
			}
		}
		TestFalse(FString::Printf(TEXT("Active Blueprint has no serialized DA_AbilityInfo reference: %s"), *RelativePath), bContainsLegacyReference);
	}
	return true;
}

#endif
