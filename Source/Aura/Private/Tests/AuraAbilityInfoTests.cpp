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
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Player/AuraPlayerState.h"
#include "Player/AuraPlayerController.h"
#include "Tests/Fixtures/AuraWidgetControllerTestReceiver.h"
#include "UI/Widget/AuraUserWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "UI/HUD/AuraHUD.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UI/WebUI/WebUIWidget.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

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
	FAuraOverlayStartupAbilityReplayTest,
	"Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraOverlayStartupAbilityReplayTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Transient test world created"), TestWorld)) return false;
	TestWorld->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraPlayerState* PlayerState = TestWorld->SpawnActor<AAuraPlayerState>(AAuraPlayerState::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("PlayerState fixture spawned"), PlayerState))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}

	UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent());
	UAttributeSet* Attributes = PlayerState->GetAttributeSet();
	if (!TestNotNull(TEXT("PlayerState has an Aura ASC"), ASC)
		|| !TestNotNull(TEXT("PlayerState has attributes"), Attributes))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}

	ASC->InitAbilityActorInfo(PlayerState, PlayerState);
	const FGameplayTag StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Status.Equipped"));
	const TArray<TPair<FGameplayTag, FGameplayTag>> ExpectedLoadout = {
		{ FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt")), FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB")) },
		{ FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBlast")), FGameplayTag::RequestGameplayTag(TEXT("InputTag.1")) },
		{ FGameplayTag::RequestGameplayTag(TEXT("Abilities.Arcane.ArcaneShards")), FGameplayTag::RequestGameplayTag(TEXT("InputTag.2")) },
		{ FGameplayTag::RequestGameplayTag(TEXT("Abilities.Lightning.Electrocute")), FGameplayTag::RequestGameplayTag(TEXT("InputTag.3")) }
	};
	for (const TPair<FGameplayTag, FGameplayTag>& ExpectedAbility : ExpectedLoadout)
	{
		FGameplayAbilitySpec AbilitySpec(UAuraDataAbility::StaticClass(), 1);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(ExpectedAbility.Key);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(ExpectedAbility.Value);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(StatusTag);
		ASC->GiveAbility(AbilitySpec);
	}
	ASC->bStartupAbilitiesGiven = true;

	TSubclassOf<UOverlayWidgetController> ControllerClass = LoadClass<UOverlayWidgetController>(
		nullptr, TEXT("/Game/Blueprints/UI/WidgetController/BP_OverlayWidgetController.BP_OverlayWidgetController_C"));
	UOverlayWidgetController* Controller = ControllerClass
		? NewObject<UOverlayWidgetController>(GetTransientPackage(), ControllerClass)
		: nullptr;
	if (!TestNotNull(TEXT("Runtime overlay controller Blueprint can be instantiated"), Controller))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}
	Controller->SetWidgetControllerParams(FWidgetControllerParams(nullptr, PlayerState, ASC, Attributes));

	// Reproduce the real failure ordering: startup specs exist and ASC callbacks are
	// bound before the overlay's spell-globe listeners receive their controller.
	Controller->BindCallbacksToDependencies();
	TSubclassOf<UAuraUserWidget> OverlayClass = LoadClass<UAuraUserWidget>(
		nullptr, TEXT("/Game/Blueprints/UI/Overlay/WBP_Overlay.WBP_Overlay_C"));
	UAuraUserWidget* Overlay = OverlayClass ? CreateWidget<UAuraUserWidget>(TestWorld, OverlayClass) : nullptr;
	if (!TestNotNull(TEXT("Real overlay Blueprint can be instantiated"), Overlay))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}
	// Match AAuraHUD: PreConstruct establishes the spell-globe input tags before
	// controller assignment and the initial ability replay.
	Overlay->TakeWidget();
	Overlay->SetWidgetController(Controller);
	auto ReadObjectProperty = [](UObject* Owner, const FName PropertyName) -> UObject*
	{
		if (!Owner) return nullptr;
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), PropertyName);
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	};
	UObject* HealthManaSpells = ReadObjectProperty(Overlay, TEXT("WBP_HealthManaSpells"));
	const TArray<TPair<FName, FGameplayTag>> ExpectedGlobes = {
		{ TEXT("SpellGlobe_LMB"), FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt")) },
		{ TEXT("SpellGlobe_1"), FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBlast")) },
		{ TEXT("SpellGlobe_2"), FGameplayTag::RequestGameplayTag(TEXT("Abilities.Arcane.ArcaneShards")) },
		{ TEXT("SpellGlobe_3"), FGameplayTag::RequestGameplayTag(TEXT("Abilities.Lightning.Electrocute")) }
	};
	if (TestNotNull(TEXT("Overlay contains the health/mana/spells widget"), HealthManaSpells))
	{
		// Adding a real overlay to the viewport can invoke PreConstruct again. The
		// spell-globe Blueprint clears its brush there, so exercise that final clear
		// before the post-attachment initial replay.
		for (const TPair<FName, FGameplayTag>& ExpectedGlobe : ExpectedGlobes)
		{
			if (UAuraUserWidget* SpellGlobe = Cast<UAuraUserWidget>(ReadObjectProperty(HealthManaSpells, ExpectedGlobe.Key)))
			{
				SpellGlobe->PreConstruct(false);
			}
		}
	}

	UAuraWidgetControllerTestReceiver* Receiver = NewObject<UAuraWidgetControllerTestReceiver>();
	Controller->AbilityInfoDelegate.AddDynamic(Receiver, &UAuraWidgetControllerTestReceiver::ReceiveAbilityInfo);
	TestEqual(TEXT("Binding dependencies does not publish before widgets bind"), Receiver->ReceivedAbilityInfo.Num(), 0);

	Controller->BroadcastInitialValues();
	TestEqual(TEXT("Initial values replay every already-replicated startup ability"), Receiver->ReceivedAbilityInfo.Num(), ExpectedLoadout.Num());
	const TArray<UObject*> AbilityInfoListeners = Controller->AbilityInfoDelegate.GetAllObjects();
	FString ListenerNames;
	for (const UObject* Listener : AbilityInfoListeners)
	{
		if (!ListenerNames.IsEmpty()) ListenerNames += TEXT(", ");
		ListenerNames += GetNameSafe(Listener);
	}
	AddInfo(FString::Printf(TEXT("AbilityInfoDelegate listeners (%d): %s"), AbilityInfoListeners.Num(), *ListenerNames));
	for (const TPair<FGameplayTag, FGameplayTag>& ExpectedAbility : ExpectedLoadout)
	{
		const FAuraAbilityInfo* ReplayedInfo = Receiver->ReceivedAbilityInfo.FindByPredicate(
			[&ExpectedAbility](const FAuraAbilityInfo& Info)
			{
				return Info.AbilityTag.MatchesTagExact(ExpectedAbility.Key);
			});
		if (TestNotNull(FString::Printf(TEXT("Startup replay includes %s"), *ExpectedAbility.Key.ToString()), ReplayedInfo))
		{
			TestEqual(FString::Printf(TEXT("%s keeps its input slot"), *ExpectedAbility.Key.ToString()), ReplayedInfo->InputTag, ExpectedAbility.Value);
			TestEqual(FString::Printf(TEXT("%s keeps its equipped status"), *ExpectedAbility.Key.ToString()), ReplayedInfo->StatusTag, StatusTag);
		}
	}

	if (TestNotNull(TEXT("Overlay contains the health/mana/spells widget"), HealthManaSpells))
	{
		for (const TPair<FName, FGameplayTag>& ExpectedGlobe : ExpectedGlobes)
		{
			UAuraUserWidget* SpellGlobe = Cast<UAuraUserWidget>(ReadObjectProperty(HealthManaSpells, ExpectedGlobe.Key));
			if (!TestNotNull(FString::Printf(TEXT("Real overlay contains %s"), *ExpectedGlobe.Key.ToString()), SpellGlobe)) continue;
			TestEqual(FString::Printf(TEXT("%s receives the overlay controller"), *ExpectedGlobe.Key.ToString()), SpellGlobe->GetWidgetController(), static_cast<UObject*>(Controller));
			const FStructProperty* InputTagProperty = FindFProperty<FStructProperty>(SpellGlobe->GetClass(), TEXT("InputTag"));
			const FGameplayTag* ActualInputTag = InputTagProperty
				? InputTagProperty->ContainerPtrToValuePtr<FGameplayTag>(SpellGlobe)
				: nullptr;
			const FGameplayTag ExpectedInputTag = ExpectedLoadout.FindByPredicate(
				[&ExpectedGlobe](const TPair<FGameplayTag, FGameplayTag>& Ability)
				{
					return Ability.Key.MatchesTagExact(ExpectedGlobe.Value);
				})->Value;
			TestTrue(
				FString::Printf(TEXT("%s keeps its configured input tag"), *ExpectedGlobe.Key.ToString()),
				ActualInputTag && ActualInputTag->MatchesTagExact(ExpectedInputTag));

			UImage* SpellIcon = Cast<UImage>(ReadObjectProperty(SpellGlobe, TEXT("Image_SpellIcon")));
			if (!TestNotNull(FString::Printf(TEXT("%s contains its icon image"), *ExpectedGlobe.Key.ToString()), SpellIcon)) continue;
			const FAuraAbilityInfo* ExpectedInfo = Receiver->ReceivedAbilityInfo.FindByPredicate(
				[&ExpectedGlobe](const FAuraAbilityInfo& Info)
				{
					return Info.AbilityTag.MatchesTagExact(ExpectedGlobe.Value);
				});
			TestTrue(
				FString::Printf(TEXT("%s renders the expected ability icon"), *ExpectedGlobe.Key.ToString()),
				ExpectedInfo && SpellIcon->GetBrush().GetResourceObject() == ExpectedInfo->Icon);
		}
	}

	FString HUDSource;
	const FString HUDSourcePath = FPaths::ProjectDir() / TEXT("Source/Aura/Private/UI/HUD/AuraHUD.cpp");
	if (TestTrue(TEXT("AuraHUD implementation is readable"), FFileHelper::LoadFileToString(HUDSource, *HUDSourcePath)))
	{
		const int32 InitOverlayOffset = HUDSource.Find(TEXT("void AAuraHUD::InitOverlay"));
		const FString InitOverlaySource = InitOverlayOffset != INDEX_NONE ? HUDSource.Mid(InitOverlayOffset) : FString();
		TestTrue(TEXT("AuraHUD initializes controllers and replays state without constructing WBP_Overlay"),
			InitOverlaySource.Contains(TEXT("GetOverlayWidgetController(WidgetControllerParams)"))
			&& InitOverlaySource.Contains(TEXT("InitializeWebHUD(PC)"))
			&& InitOverlaySource.Contains(TEXT("BroadcastInitialValues()"))
			&& !InitOverlaySource.Contains(TEXT("CreateWidget<UUserWidget>"))
			&& !InitOverlaySource.Contains(TEXT("OverlayWidget->AddToViewport")));
	}

	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebSkillPanelHUDContractTest,
	"Aura.UI.WebSkillPanel.HUDContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebSkillPanelHUDContractTest::RunTest(const FString& Parameters)
{
	FString HUDSource;
	const FString HUDSourcePath = FPaths::ProjectDir() / TEXT("Source/Aura/Private/UI/HUD/AuraHUD.cpp");
	if (!TestTrue(TEXT("AuraHUD implementation is readable for the web skill-panel contract"), FFileHelper::LoadFileToString(HUDSource, *HUDSourcePath)))
	{
		return false;
	}

	TestTrue(TEXT("HUD creates four dedicated bounded WebUI panels"),
		HUDSource.Contains(TEXT("WebUI/hud-left-top.html"))
		&& HUDSource.Contains(TEXT("WebUI/hud-right-top.html"))
		&& HUDSource.Contains(TEXT("WebUI/hud-bottom.html"))
		&& HUDSource.Contains(TEXT("WebUI/hud-interaction.html"))
		&& HUDSource.Contains(TEXT("CreateWebHUDPanel")));
	TestTrue(TEXT("HUD uses non-overlapping panel geometry around the level viewport"),
		HUDSource.Contains(TEXT("FAnchors(0.f, 0.f, 0.f, 0.f)"))
		&& HUDSource.Contains(TEXT("FAnchors(1.f, 0.f, 1.f, 0.f)"))
		&& HUDSource.Contains(TEXT("FAnchors(0.f, 1.f, 1.f, 1.f)"))
		&& HUDSource.Contains(TEXT("FMargin(-390.f, 24.f, 366.f, 210.f)"))
		&& HUDSource.Contains(TEXT("FMargin(24.f, -140.f, 24.f, 116.f)"))
		&& HUDSource.Contains(TEXT("FMargin(-260.f, 32.f, 520.f, 210.f)")));
	TestTrue(TEXT("HUD publishes ability state through the bridge"), HUDSource.Contains(TEXT("skill_panel_ability")) && HUDSource.Contains(TEXT("HandleAbilityInfoForWebUI")));
	TestTrue(TEXT("HUD sends authored PNG skill icons through the bridge"),
		HUDSource.Contains(TEXT("BuildManualSkillIconDataUri"))
		&& HUDSource.Contains(TEXT("firebolt.png"))
		&& HUDSource.Contains(TEXT("gunfire.png"))
		&& HUDSource.Contains(TEXT("electrocute.png"))
		&& HUDSource.Contains(TEXT("fireblast.png"))
		&& HUDSource.Contains(TEXT("arcaneshards.png"))
		&& HUDSource.Contains(TEXT("haloofprotection.png"))
		&& HUDSource.Contains(TEXT("lifesiphon.png"))
		&& HUDSource.Contains(TEXT("manasiphon.png")));
	TestTrue(TEXT("HUD publishes health and mana state through the bridge"),
		HUDSource.Contains(TEXT("hud_vitals"))
		&& HUDSource.Contains(TEXT("HandleHealthChangedForWebUI"))
		&& HUDSource.Contains(TEXT("HandleManaChangedForWebUI")));
	TestTrue(TEXT("HUD replays all controller state after the browser reports ready"), HUDSource.Contains(TEXT("hud_ready")) && HUDSource.Contains(TEXT("BroadcastAbilityInfo()")) && HUDSource.Contains(TEXT("BroadcastInitialValues()")));
	TestTrue(TEXT("HUD replays the authoritative snapshot on a late browser connection and falls back to PlayerState role during pawn replication"),
		HUDSource.Contains(TEXT("if (bConnected)"))
		&& HUDSource.Contains(TEXT("SendInitialWebHUDState();"))
		&& HUDSource.Contains(TEXT("const FName DisplayRoleId"))
		&& HUDSource.Contains(TEXT("PS ? PS->GetRole() : NAME_None")));
	TestTrue(TEXT("HUD reloads each bounded page after mounting at a top viewport layer"),
		HUDSource.Contains(TEXT("Panel->AddToViewport(200)"))
		&& HUDSource.Contains(TEXT("Panel->ReloadWebUI()"))
		&& HUDSource.Contains(TEXT("Panel->bAutoConnectBridge = false")));
	TestTrue(TEXT("HUD uses the local player viewport context with a headless world fallback"),
		HUDSource.Contains(TEXT("PC->GetLocalPlayer()"))
		&& HUDSource.Contains(TEXT("CreateWidget<UWebUIWidget>(PC"))
		&& HUDSource.Contains(TEXT("CreateWidget<UWebUIWidget>(World")));
	TestTrue(TEXT("HUD does not instantiate or toggle the legacy native overlay"),
		!HUDSource.Contains(TEXT("CreateWidget<UUserWidget>"))
		&& !HUDSource.Contains(TEXT("SetNativeSkillGlobeVisibility"))
		&& !HUDSource.Contains(TEXT("TriggerNativeOverlayButton"))
		&& !HUDSource.Contains(TEXT("OnClicked.Broadcast")));
	TestTrue(TEXT("HUD routes web press, held, and release commands to the player controller"),
		HUDSource.Contains(TEXT("skill_ability_pressed"))
		&& HUDSource.Contains(TEXT("skill_ability_held"))
		&& HUDSource.Contains(TEXT("skill_ability_released"))
		&& HUDSource.Contains(TEXT("WebAbilityInputTagPressed"))
		&& HUDSource.Contains(TEXT("WebAbilityInputTagHeld"))
		&& HUDSource.Contains(TEXT("WebAbilityInputTagReleased")));
	TestTrue(TEXT("HUD releases WebUI-originated LMB input when the browser disconnects"),
		HUDSource.Contains(TEXT("bWebGameplayLMBDown"))
		&& HUDSource.Contains(TEXT("HandleWebUIConnectionChanged"))
		&& HUDSource.Contains(TEXT("InputTag_LMB")));
	TestTrue(TEXT("HUD leaves level mouse clicks to the native viewport instead of synthesizing them from a full-screen browser"),
		!HUDSource.Contains(TEXT("startGameplayLmb"))
		&& HUDSource.Contains(TEXT("SetWebHUDInteractionLayout"))
		&& HUDSource.Contains(TEXT("SetWebHUDMenuLayout")));
	TestTrue(TEXT("HUD routes Web UI menu controls through controller-backed WebUI commands"),
		HUDSource.Contains(TEXT("hud_attributes_clicked"))
		&& HUDSource.Contains(TEXT("hud_spells_clicked"))
		&& HUDSource.Contains(TEXT("hud_close_clicked"))
		&& HUDSource.Contains(TEXT("hud_quit_confirm"))
		&& HUDSource.Contains(TEXT("hud_spell_slot"))
		&& HUDSource.Contains(TEXT("hud_attribute_upgrade")));
	TestTrue(TEXT("HUD publishes interaction previews and location through WebUI"),
		HUDSource.Contains(TEXT("hud_interaction"))
		&& HUDSource.Contains(TEXT("hud_location"))
		&& HUDSource.Contains(TEXT("WebInteractPressed")));
	TestTrue(TEXT("HUD restricts browser input to known gameplay input tags"), HUDSource.Contains(TEXT("InputTag_Passive_2")));
	const UClass* HUDClass = AAuraHUD::StaticClass();
	TestNotNull(TEXT("Web command handler is reflected for dynamic delegate binding"), HUDClass->FindFunctionByName(TEXT("HandleWebUICommand")));
	TestNotNull(TEXT("Web connection handler is reflected for dynamic delegate binding"), HUDClass->FindFunctionByName(TEXT("HandleWebUIConnectionChanged")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebSkillPanelHUDRuntimeTest,
	"Aura.UI.WebSkillPanel.RuntimeMountAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebSkillPanelHUDRuntimeTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Transient Web HUD test world created"), TestWorld))
	{
		return false;
	}
	TestWorld->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraPlayerState* PlayerState = TestWorld->SpawnActor<AAuraPlayerState>(AAuraPlayerState::StaticClass(), FTransform::Identity, SpawnParams);
	AAuraPlayerController* PlayerController = TestWorld->SpawnActor<AAuraPlayerController>(AAuraPlayerController::StaticClass(), FTransform::Identity, SpawnParams);
	AAuraHUD* HUD = TestWorld->SpawnActor<AAuraHUD>(AAuraHUD::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Web HUD PlayerState fixture spawned"), PlayerState)
		|| !TestNotNull(TEXT("Web HUD local PlayerController fixture spawned"), PlayerController)
		|| !TestNotNull(TEXT("Web HUD fixture spawned"), HUD))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}

	const TSubclassOf<UOverlayWidgetController> ControllerClass = LoadClass<UOverlayWidgetController>(
		nullptr, TEXT("/Game/Blueprints/UI/WidgetController/BP_OverlayWidgetController.BP_OverlayWidgetController_C"));
	const TSubclassOf<UAttributeMenuWidgetController> AttributeControllerClass = LoadClass<UAttributeMenuWidgetController>(
		nullptr, TEXT("/Game/Blueprints/UI/WidgetController/BP_AttributeMenuWidgetController.BP_AttributeMenuWidgetController_C"));
	const TSubclassOf<USpellMenuWidgetController> SpellControllerClass = LoadClass<USpellMenuWidgetController>(
		nullptr, TEXT("/Game/Blueprints/UI/WidgetController/BP_SpellMenuWidgetController.BP_SpellMenuWidgetController_C"));
	if (!TestNotNull(TEXT("Runtime overlay controller class loaded for Web HUD"), ControllerClass.Get())
		|| !TestNotNull(TEXT("Runtime attribute controller class loaded for Web HUD"), AttributeControllerClass.Get())
		|| !TestNotNull(TEXT("Runtime spell controller class loaded for Web HUD"), SpellControllerClass.Get()))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}

	auto SetObjectProperty = [](UObject* Owner, const FName PropertyName, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), PropertyName))
		{
			Property->SetObjectPropertyValue_InContainer(Owner, Value);
		}
	};
	SetObjectProperty(HUD, TEXT("OverlayWidgetControllerClass"), ControllerClass.Get());
	SetObjectProperty(HUD, TEXT("AttributeMenuWidgetControllerClass"), AttributeControllerClass.Get());
	SetObjectProperty(HUD, TEXT("SpellMenuWidgetControllerClass"), SpellControllerClass.Get());

	UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent());
	UAttributeSet* Attributes = PlayerState->GetAttributeSet();
	if (!TestNotNull(TEXT("Web HUD PlayerState has an Aura ASC"), ASC)
		|| !TestNotNull(TEXT("Web HUD PlayerState has attributes"), Attributes))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}
	ASC->InitAbilityActorInfo(PlayerState, PlayerState);

	HUD->InitOverlay(PlayerController, PlayerState, ASC, Attributes);
	auto ReadObjectProperty = [](UObject* Owner, const FName PropertyName) -> UObject*
	{
		if (!Owner) return nullptr;
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), PropertyName);
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	};

	UWebUIWidget* WebHUDLeftTop = Cast<UWebUIWidget>(ReadObjectProperty(HUD, TEXT("WebHUDLeftTop")));
	UWebUIWidget* WebHUDRightTop = Cast<UWebUIWidget>(ReadObjectProperty(HUD, TEXT("WebHUDRightTop")));
	UWebUIWidget* WebHUDBottom = Cast<UWebUIWidget>(ReadObjectProperty(HUD, TEXT("WebHUDBottom")));
	UWebUIWidget* WebHUDInteraction = Cast<UWebUIWidget>(ReadObjectProperty(HUD, TEXT("WebHUDInteraction")));
	UWebUIBridgeSubsystem* Bridge = TestWorld->GetSubsystem<UWebUIBridgeSubsystem>();
	TestNotNull(TEXT("HUD mounts the left-top WebUI panel"), WebHUDLeftTop);
	TestNotNull(TEXT("HUD mounts the right-top WebUI panel"), WebHUDRightTop);
	TestNotNull(TEXT("HUD mounts the bottom WebUI panel"), WebHUDBottom);
	TestNotNull(TEXT("HUD mounts the separate interaction WebUI panel"), WebHUDInteraction);
	TestNotNull(TEXT("HUD runtime fixture owns the Web UI bridge"), Bridge);
	if (WebHUDLeftTop && WebHUDRightTop && WebHUDBottom && WebHUDInteraction && Bridge)
	{
		TestTrue(TEXT("HUD binds its Web UI command handler to the live bridge"), Bridge->OnCommand.IsBound());
		const TArray<UObject*> CommandListeners = Bridge->OnCommand.GetAllObjects();
		FString CommandListenerNames;
		for (const UObject* Listener : CommandListeners)
		{
			if (!CommandListenerNames.IsEmpty()) CommandListenerNames += TEXT(", ");
			CommandListenerNames += GetNameSafe(Listener);
		}
		AddInfo(FString::Printf(TEXT("Web UI command listeners (%d): %s"), CommandListeners.Num(), *CommandListenerNames));
		TestNotNull(TEXT("Mounted left-top panel creates a native WebBrowser root"), WebHUDLeftTop->GetWebBrowser());
		TestNotNull(TEXT("Mounted right-top panel creates a native WebBrowser root"), WebHUDRightTop->GetWebBrowser());
		TestNotNull(TEXT("Mounted bottom panel creates a native WebBrowser root"), WebHUDBottom->GetWebBrowser());
		TestNotNull(TEXT("Mounted interaction panel creates a native WebBrowser root"), WebHUDInteraction->GetWebBrowser());
		TestTrue(TEXT("All HUD panels enable transparent browser compositing"),
			WebHUDLeftTop->IsBrowserTransparencyEnabled()
			&& WebHUDRightTop->IsBrowserTransparencyEnabled()
			&& WebHUDBottom->IsBrowserTransparencyEnabled()
			&& WebHUDInteraction->IsBrowserTransparencyEnabled());
		TestEqual(TEXT("Left-top panel loads its dedicated page"), WebHUDLeftTop->GetLastLoadedHtmlAssetPath(), FString(TEXT("WebUI/hud-left-top.html")));
		TestEqual(TEXT("Right-top panel loads its dedicated page"), WebHUDRightTop->GetLastLoadedHtmlAssetPath(), FString(TEXT("WebUI/hud-right-top.html")));
		TestEqual(TEXT("Bottom panel loads its dedicated page"), WebHUDBottom->GetLastLoadedHtmlAssetPath(), FString(TEXT("WebUI/hud-bottom.html")));
		TestEqual(TEXT("Interaction panel loads its dedicated page"), WebHUDInteraction->GetLastLoadedHtmlAssetPath(), FString(TEXT("WebUI/hud-interaction.html")));
		TestTrue(TEXT("All HUD panels load non-empty HTML"),
			WebHUDLeftTop->GetLastLoadedHtmlBytes() > 1000
			&& WebHUDRightTop->GetLastLoadedHtmlBytes() > 1000
			&& WebHUDBottom->GetLastLoadedHtmlBytes() > 1000
			&& WebHUDInteraction->GetLastLoadedHtmlBytes() > 1000);

		if (UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get(TestWorld))
		{
			if (ViewportSubsystem->IsWidgetAdded(WebHUDLeftTop)
				&& ViewportSubsystem->IsWidgetAdded(WebHUDRightTop)
				&& ViewportSubsystem->IsWidgetAdded(WebHUDBottom)
				&& ViewportSubsystem->IsWidgetAdded(WebHUDInteraction))
			{
				const FGameViewportWidgetSlot LeftTopSlot = ViewportSubsystem->GetWidgetSlot(WebHUDLeftTop);
				const FGameViewportWidgetSlot RightTopSlot = ViewportSubsystem->GetWidgetSlot(WebHUDRightTop);
				const FGameViewportWidgetSlot BottomSlot = ViewportSubsystem->GetWidgetSlot(WebHUDBottom);
				const FGameViewportWidgetSlot InteractionSlot = ViewportSubsystem->GetWidgetSlot(WebHUDInteraction);
				TestEqual(TEXT("All Web HUD panels are mounted above native HUD layers"), LeftTopSlot.ZOrder, 200);
				TestEqual(TEXT("Right-top panel shares the Web HUD z-order"), RightTopSlot.ZOrder, 200);
				TestEqual(TEXT("Bottom panel shares the Web HUD z-order"), BottomSlot.ZOrder, 200);
				TestEqual(TEXT("Interaction panel shares the Web HUD z-order"), InteractionSlot.ZOrder, 200);
				TestTrue(TEXT("Web HUD panels use bounded, non-fullscreen geometry"),
					LeftTopSlot.Anchors.Minimum.X == 0.f && LeftTopSlot.Anchors.Maximum.X == 0.f
					&& RightTopSlot.Anchors.Minimum.X == 1.f && RightTopSlot.Anchors.Maximum.X == 1.f
					&& BottomSlot.Anchors.Minimum.Y == 1.f && BottomSlot.Anchors.Maximum.Y == 1.f
					&& InteractionSlot.Anchors.Minimum.X == 0.5f && InteractionSlot.Anchors.Maximum.X == 0.5f
					&& RightTopSlot.Offsets.Right > 0.f && RightTopSlot.Offsets.Bottom > 0.f
					&& BottomSlot.Offsets.Right > 0.f && BottomSlot.Offsets.Top == -140.f
					&& BottomSlot.Offsets.Bottom >= 116.f
					&& InteractionSlot.Offsets.Right == 520.f && InteractionSlot.Offsets.Bottom == 210.f);
			}
			else
			{
			AddInfo(TEXT("Headless automation has no game viewport; verified the mounted Web HUD host, browser root, and loaded page instead."));
			}
		}
		TestNotNull(TEXT("HUD exposes the reflected WebUI command function"), HUD->GetClass()->FindFunctionByName(TEXT("HandleWebUICommand")));
		TestNotNull(TEXT("HUD exposes the reflected WebUI connection function"), HUD->GetClass()->FindFunctionByName(TEXT("HandleWebUIConnectionChanged")));
		HUD->HandleWebUICommand(TEXT("hud_ready"), TEXT("{}"));
		HUD->HandleWebUIConnectionChanged(false);
		TestEqual(TEXT("Web HUD never forwards controls to native WBP handlers"), HUD->GetWebHudForwardedActionCount(), 0);
	}

	HUD->Destroy();
	PlayerController->Destroy();
	PlayerState->Destroy();
	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
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
