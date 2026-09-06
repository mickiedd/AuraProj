// Copyright Druid Mechanics

#include "UI/HUD/AuraHUD.h"

#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "Battle/AuraBattleDirector.h"
#include "Character/AuraCharacterBase.h"
#include "Character/AuraCivilian.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Dom/JsonObject.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraInventoryComponent.h"
#include "Economy/AuraMerchantComponent.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Interaction/AuraInteractionComponent.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Player/AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UI/WebUI/WebUIWidget.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "UI/WidgetController/TargetInteractionWidgetController.h"
#include "WebBrowser.h"

namespace AuraHUDPrivate
{
	FString SerializeObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Object, Writer);
		Writer->Close();
		return Json;
	}

	void SetTagField(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FGameplayTag& Tag)
	{
		Object->SetStringField(FieldName, Tag.IsValid() ? Tag.ToString() : FString());
	}

	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}
		return FString();
	}

	const TCHAR* GetManualSkillIconFilename(const FGameplayTag& AbilityTag)
	{
		const FString TagName = AbilityTag.ToString();
		if (TagName == TEXT("Abilities.None")) return TEXT("empty.png");
		if (TagName == TEXT("Abilities.Fire.FireBolt")) return TEXT("firebolt.png");
		if (TagName == TEXT("Abilities.Gun.Fire")) return TEXT("gunfire.png");
		if (TagName == TEXT("Abilities.Lightning.Electrocute")) return TEXT("electrocute.png");
		if (TagName == TEXT("Abilities.Fire.FireBlast")) return TEXT("fireblast.png");
		if (TagName == TEXT("Abilities.Arcane.ArcaneShards")) return TEXT("arcaneshards.png");
		if (TagName == TEXT("Abilities.Passive.HaloOfProtection")) return TEXT("haloofprotection.png");
		if (TagName == TEXT("Abilities.Passive.LifeSiphon")) return TEXT("lifesiphon.png");
		if (TagName == TEXT("Abilities.Passive.ManaSiphon")) return TEXT("manasiphon.png");
		return nullptr;
	}
}

UOverlayWidgetController* AAuraHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		const UClass* ControllerClass = OverlayWidgetControllerClass
			? OverlayWidgetControllerClass.Get()
			: UOverlayWidgetController::StaticClass();
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, ControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

void AAuraHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bWebGameplayLMBDown)
	{
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
		{
			AuraPC->WebAbilityInputTagReleased(FAuraGameplayTags::Get().InputTag_LMB);
		}
		bWebGameplayLMBDown = false;
	}

	if (OverlayWidgetController)
	{
		OverlayWidgetController->AbilityInfoDelegate.RemoveDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
		OverlayWidgetController->OnHealthChanged.RemoveDynamic(this, &AAuraHUD::HandleHealthChangedForWebUI);
		OverlayWidgetController->OnMaxHealthChanged.RemoveDynamic(this, &AAuraHUD::HandleMaxHealthChangedForWebUI);
		OverlayWidgetController->OnManaChanged.RemoveDynamic(this, &AAuraHUD::HandleManaChangedForWebUI);
		OverlayWidgetController->OnMaxManaChanged.RemoveDynamic(this, &AAuraHUD::HandleMaxManaChangedForWebUI);
		OverlayWidgetController->MessageWidgetRowDelegate.RemoveDynamic(this, &AAuraHUD::HandleMessageForWebUI);
		OverlayWidgetController->OnXPPercentChangedDelegate.RemoveDynamic(this, &AAuraHUD::HandleXPPercentChangedForWebUI);
		OverlayWidgetController->OnPlayerLevelChangedDelegate.RemoveDynamic(this, &AAuraHUD::HandlePlayerLevelChangedForWebUI);
		OverlayWidgetController->OnMountedChangedDelegate.RemoveDynamic(this, &AAuraHUD::HandleMountedChangedForWebUI);
	}

	if (AttributeMenuWidgetController)
	{
		AttributeMenuWidgetController->AttributeInfoDelegate.RemoveDynamic(this, &AAuraHUD::HandleAttributeInfoForWebUI);
		AttributeMenuWidgetController->AttributePointsChangedDelegate.RemoveDynamic(this, &AAuraHUD::HandleAttributePointsChangedForWebUI);
	}

	if (SpellMenuWidgetController)
	{
		SpellMenuWidgetController->AbilityInfoDelegate.RemoveDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
		SpellMenuWidgetController->SpellPointsChanged.RemoveDynamic(this, &AAuraHUD::HandleSpellPointsChangedForWebUI);
		SpellMenuWidgetController->SpellGlobeSelectedDelegate.RemoveDynamic(this, &AAuraHUD::HandleSpellSelectionForWebUI);
		SpellMenuWidgetController->WaitForEquipDelegate.RemoveDynamic(this, &AAuraHUD::HandleWaitForEquipForWebUI);
		SpellMenuWidgetController->StopWaitingForEquipDelegate.RemoveDynamic(this, &AAuraHUD::HandleStopWaitingForEquipForWebUI);
		SpellMenuWidgetController->SpellGlobeReassignedDelegate.RemoveDynamic(this, &AAuraHUD::HandleSpellReassignedForWebUI);
	}

	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		if (UTargetInteractionWidgetController* TargetController = AuraPC->GetTargetInteractionWidgetController())
		{
			TargetController->OnTargetPreview.RemoveDynamic(this, &AAuraHUD::HandleTargetPreviewForWebUI);
			TargetController->OnTargetPreviewCleared.RemoveDynamic(this, &AAuraHUD::HandleTargetPreviewClearedForWebUI);
		}
		if (UAuraInteractionComponent* Interaction = AuraPC->GetInteractionComponent())
		{
			Interaction->OnPurchaseResult.RemoveAll(this);
		}
		if (AAuraPlayerState* PlayerState = AuraPC->GetPlayerState<AAuraPlayerState>())
		{
			if (PlayerState->GetCurrencyComponent()) PlayerState->GetCurrencyComponent()->OnCurrencyChanged.RemoveAll(this);
			if (PlayerState->GetInventoryComponent()) PlayerState->GetInventoryComponent()->OnInventoryChanged.RemoveAll(this);
			PlayerState->OnRoleChangedDelegate.RemoveAll(this);
			PlayerState->OnFirearmStateChanged.RemoveAll(this);
			PlayerState->OnTutorialProgressChanged.RemoveAll(this);
		}
	}

	if (WebUIBridge)
	{
		WebUIBridge->OnCommand.RemoveDynamic(this, &AAuraHUD::HandleWebUICommand);
		WebUIBridge->OnConnectionChanged.RemoveDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);
	}

	if (WebHUDLeftTop && IsValid(WebHUDLeftTop)) WebHUDLeftTop->RemoveFromParent();
	if (WebHUDRightTop && IsValid(WebHUDRightTop)) WebHUDRightTop->RemoveFromParent();
	if (WebHUDBottom && IsValid(WebHUDBottom)) WebHUDBottom->RemoveFromParent();
	if (WebHUDInteraction && IsValid(WebHUDInteraction)) WebHUDInteraction->RemoveFromParent();
	WebHUDLeftTop = nullptr;
	WebHUDRightTop = nullptr;
	WebHUDBottom = nullptr;
	WebHUDInteraction = nullptr;
	WebUIBridge = nullptr;
	bWebHUDReady = false;
	bWebInteractionVisible = false;
	AbilityIconDataUriCache.Empty();
	ManualSkillIconDataUriCache.Empty();
	LastInteractionPayloadJson.Empty();
	LastLocationPayloadJson.Empty();
	LastRoleStatePayloadJson.Empty();
	LastBattleStatePayloadJson.Empty();
	LastMerchantPayloadJson.Empty();
	WebMerchantComponent = nullptr;
	bMerchantUIOpen = false;

	Super::EndPlay(EndPlayReason);
}

UAttributeMenuWidgetController* AAuraHUD::GetAttributeMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (AttributeMenuWidgetController == nullptr)
	{
		const UClass* ControllerClass = AttributeMenuWidgetControllerClass
			? AttributeMenuWidgetControllerClass.Get()
			: UAttributeMenuWidgetController::StaticClass();
		AttributeMenuWidgetController = NewObject<UAttributeMenuWidgetController>(this, ControllerClass);
		AttributeMenuWidgetController->SetWidgetControllerParams(WCParams);
		AttributeMenuWidgetController->BindCallbacksToDependencies();
	}
	return AttributeMenuWidgetController;
}

USpellMenuWidgetController* AAuraHUD::GetSpellMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (SpellMenuWidgetController == nullptr)
	{
		const UClass* ControllerClass = SpellMenuWidgetControllerClass
			? SpellMenuWidgetControllerClass.Get()
			: USpellMenuWidgetController::StaticClass();
		SpellMenuWidgetController = NewObject<USpellMenuWidgetController>(this, ControllerClass);
		SpellMenuWidgetController->SetWidgetControllerParams(WCParams);
		SpellMenuWidgetController->BindCallbacksToDependencies();
	}
	return SpellMenuWidgetController;
}

void AAuraHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	if (!PC || !PS || !ASC || !AS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web HUD initialization skipped because the controller parameters are incomplete"));
		return;
	}

	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* OverlayController = GetOverlayWidgetController(WidgetControllerParams);
	UAttributeMenuWidgetController* AttributeController = GetAttributeMenuWidgetController(WidgetControllerParams);
	USpellMenuWidgetController* SpellController = GetSpellMenuWidgetController(WidgetControllerParams);
	InitializeWebHUD(PC);

	// Controllers remain the authoritative GAS-facing data/action layer. They no
	// longer own or feed native widgets; the WebUI receives their initial replay.
	if (OverlayController) OverlayController->BroadcastInitialValues();
	if (AttributeController) AttributeController->BroadcastInitialValues();
	if (SpellController) SpellController->BroadcastInitialValues();
	SendInitialWebHUDState();
}

void AAuraHUD::InitializeWebHUD(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController() || WebHUDLeftTop || WebHUDRightTop || WebHUDBottom || WebHUDInteraction) return;

	UWorld* World = GetWorld();
	if (!World) return;
	WebUIBridge = World->GetSubsystem<UWebUIBridgeSubsystem>();
	if (!WebUIBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web HUD skipped: bridge subsystem unavailable"));
		return;
	}

	// Use the local-player context in gameplay so each browser shares the active
	// viewport. The world context keeps headless editor automation constructible.
	WebHUDLeftTop = CreateWebHUDPanel(
		PC,
		TEXT("WebUI/hud-left-top.html"),
		FAnchors(0.f, 0.f, 0.f, 0.f),
		FMargin(24.f, 24.f, 360.f, 250.f),
		TEXT("left-top"));
	WebHUDRightTop = CreateWebHUDPanel(
		PC,
		TEXT("WebUI/hud-right-top.html"),
		FAnchors(1.f, 0.f, 1.f, 0.f),
		// Non-stretched axes use Right/Bottom as fixed size, not CSS-style margins.
		FMargin(-390.f, 24.f, 366.f, 210.f),
		TEXT("right-top"));
	WebHUDBottom = CreateWebHUDPanel(
		PC,
		TEXT("WebUI/hud-bottom.html"),
		FAnchors(0.f, 1.f, 1.f, 1.f),
		// The skill strip is 102px tall at the widest responsive tile size.
		// Reserve additional browser height so its bottom border and labels are
		// not clipped by the native WebBrowser host.
		FMargin(24.f, -140.f, 24.f, 116.f),
		TEXT("bottom"));
	WebHUDInteraction = CreateWebHUDPanel(
		PC,
		TEXT("WebUI/hud-interaction.html"),
		FAnchors(0.5f, 0.f, 0.5f, 0.f),
		FMargin(-260.f, 32.f, 520.f, 210.f),
		TEXT("interaction"));
	if (!WebHUDLeftTop || !WebHUDRightTop || !WebHUDBottom || !WebHUDInteraction)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraHUD] Failed to create all three bounded WebUI HUD panels"));
		if (WebHUDLeftTop && IsValid(WebHUDLeftTop)) WebHUDLeftTop->RemoveFromParent();
		if (WebHUDRightTop && IsValid(WebHUDRightTop)) WebHUDRightTop->RemoveFromParent();
		if (WebHUDBottom && IsValid(WebHUDBottom)) WebHUDBottom->RemoveFromParent();
		if (WebHUDInteraction && IsValid(WebHUDInteraction)) WebHUDInteraction->RemoveFromParent();
		WebHUDLeftTop = nullptr;
		WebHUDRightTop = nullptr;
		WebHUDBottom = nullptr;
		WebHUDInteraction = nullptr;
		WebUIBridge = nullptr;
		return;
	}

	if (OverlayWidgetController)
	{
		OverlayWidgetController->AbilityInfoDelegate.AddDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
		OverlayWidgetController->OnHealthChanged.AddDynamic(this, &AAuraHUD::HandleHealthChangedForWebUI);
		OverlayWidgetController->OnMaxHealthChanged.AddDynamic(this, &AAuraHUD::HandleMaxHealthChangedForWebUI);
		OverlayWidgetController->OnManaChanged.AddDynamic(this, &AAuraHUD::HandleManaChangedForWebUI);
		OverlayWidgetController->OnMaxManaChanged.AddDynamic(this, &AAuraHUD::HandleMaxManaChangedForWebUI);
		OverlayWidgetController->MessageWidgetRowDelegate.AddDynamic(this, &AAuraHUD::HandleMessageForWebUI);
		OverlayWidgetController->OnXPPercentChangedDelegate.AddDynamic(this, &AAuraHUD::HandleXPPercentChangedForWebUI);
		OverlayWidgetController->OnPlayerLevelChangedDelegate.AddDynamic(this, &AAuraHUD::HandlePlayerLevelChangedForWebUI);
		OverlayWidgetController->OnMountedChangedDelegate.AddDynamic(this, &AAuraHUD::HandleMountedChangedForWebUI);
	}
	if (AttributeMenuWidgetController)
	{
		AttributeMenuWidgetController->AttributeInfoDelegate.AddDynamic(this, &AAuraHUD::HandleAttributeInfoForWebUI);
		AttributeMenuWidgetController->AttributePointsChangedDelegate.AddDynamic(this, &AAuraHUD::HandleAttributePointsChangedForWebUI);
	}
	if (SpellMenuWidgetController)
	{
		SpellMenuWidgetController->AbilityInfoDelegate.AddDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
		SpellMenuWidgetController->SpellPointsChanged.AddDynamic(this, &AAuraHUD::HandleSpellPointsChangedForWebUI);
		SpellMenuWidgetController->SpellGlobeSelectedDelegate.AddDynamic(this, &AAuraHUD::HandleSpellSelectionForWebUI);
		SpellMenuWidgetController->WaitForEquipDelegate.AddDynamic(this, &AAuraHUD::HandleWaitForEquipForWebUI);
		SpellMenuWidgetController->StopWaitingForEquipDelegate.AddDynamic(this, &AAuraHUD::HandleStopWaitingForEquipForWebUI);
		SpellMenuWidgetController->SpellGlobeReassignedDelegate.AddDynamic(this, &AAuraHUD::HandleSpellReassignedForWebUI);
	}
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(PC))
	{
		if (UTargetInteractionWidgetController* TargetController = AuraPC->GetTargetInteractionWidgetController())
		{
			TargetController->OnTargetPreview.AddDynamic(this, &AAuraHUD::HandleTargetPreviewForWebUI);
			TargetController->OnTargetPreviewCleared.AddDynamic(this, &AAuraHUD::HandleTargetPreviewClearedForWebUI);
		}
		if (UAuraInteractionComponent* Interaction = AuraPC->GetInteractionComponent())
		{
			Interaction->OnPurchaseResult.RemoveAll(this);
			Interaction->OnPurchaseResult.AddUObject(this, &AAuraHUD::HandlePurchaseResultForWebUI);
		}
		if (AAuraPlayerState* PlayerState = AuraPC->GetPlayerState<AAuraPlayerState>())
		{
			if (UAuraCurrencyComponent* Currency = PlayerState->GetCurrencyComponent())
			{
				Currency->OnCurrencyChanged.RemoveAll(this);
				Currency->OnCurrencyChanged.AddUObject(this, &AAuraHUD::HandleCurrencyChangedForWebUI);
			}
			if (UAuraInventoryComponent* Inventory = PlayerState->GetInventoryComponent())
			{
				Inventory->OnInventoryChanged.RemoveAll(this);
				Inventory->OnInventoryChanged.AddUObject(this, &AAuraHUD::HandleInventoryChangedForWebUI);
			}
			PlayerState->OnRoleChangedDelegate.RemoveAll(this);
			PlayerState->OnRoleChangedDelegate.AddUObject(this, &AAuraHUD::HandleRoleChangedForWebUI);
			PlayerState->OnFirearmStateChanged.RemoveAll(this);
			PlayerState->OnFirearmStateChanged.AddUObject(this, &AAuraHUD::HandleFirearmStateForWebUI);
			PlayerState->OnTutorialProgressChanged.RemoveAll(this);
			PlayerState->OnTutorialProgressChanged.AddUObject(this, &AAuraHUD::HandleTutorialProgressForWebUI);
		}
	}

	WebUIBridge->OnCommand.AddDynamic(this, &AAuraHUD::HandleWebUICommand);
	WebUIBridge->OnConnectionChanged.AddDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Bounded WebUI HUD mounted: left-top=%s right-top=%s bottom=%s interaction=%s context=%s"),
		*GetNameSafe(WebHUDLeftTop->GetWebBrowser()),
		*GetNameSafe(WebHUDRightTop->GetWebBrowser()),
		*GetNameSafe(WebHUDBottom->GetWebBrowser()),
		*GetNameSafe(WebHUDInteraction->GetWebBrowser()),
		PC->GetLocalPlayer() ? TEXT("LocalPlayer") : TEXT("WorldFallback"));
}

UWebUIWidget* AAuraHUD::CreateWebHUDPanel(APlayerController* PC, const FString& HtmlPath, const FAnchors& Anchors, const FMargin& Offsets, const TCHAR* PanelName)
{
	if (!PC) return nullptr;
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	UWebUIWidget* Panel = PC->GetLocalPlayer()
		? CreateWidget<UWebUIWidget>(PC, UWebUIWidget::StaticClass())
		: CreateWidget<UWebUIWidget>(World, UWebUIWidget::StaticClass());
	if (!Panel)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraHUD] Failed to create WebUI HUD panel '%s'"), PanelName);
		return nullptr;
	}

	Panel->HtmlAssetPath = HtmlPath;
	Panel->bAutoConnectBridge = false;
	Panel->ConfigureViewportLayout(Anchors, Offsets, FVector2D(0.f, 0.f));
	Panel->AddToViewport(200);
	Panel->ReloadWebUI();
	Panel->SetVisibility(ESlateVisibility::Visible);
	return Panel;
}

void AAuraHUD::SetWebHUDMenuLayout(bool bExpanded)
{
	if (!WebHUDRightTop) return;
	if (bExpanded)
	{
		WebHUDRightTop->ConfigureViewportLayout(FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f), FVector2D(0.f, 0.f));
	}
	else
	{
		WebHUDRightTop->ConfigureViewportLayout(FAnchors(1.f, 0.f, 1.f, 0.f), FMargin(-390.f, 24.f, 366.f, 210.f), FVector2D(0.f, 0.f));
	}
}

void AAuraHUD::SetWebHUDInteractionLayout(bool bExpanded)
{
	if (!WebHUDInteraction) return;
	WebHUDInteraction->SetVisibility(bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void AAuraHUD::SendInitialWebHUDState()
{
	if (!WebUIBridge) return;
	SendVitalsToWebUI();
	SendPlayerProgressToWebUI();
	SendAttributeCatalogToWebUI();
	SendSpellCatalogToWebUI();
	SendLocationToWebUI();
	SendRoleStateToWebUI();
	SendBattleStateToWebUI();
	SendEconomyStateToWebUI();
	SendFirearmStateToWebUI();
	SendTutorialStateToWebUI();
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		const FAuraTargetDescriptor& Descriptor = AuraPC->GetFocusedTargetDescriptor();
		if (Descriptor.IsValid()) SendInteractionToWebUI(Descriptor);
		else HandleTargetPreviewClearedForWebUI();
	}
}

void AAuraHUD::HandleWebUICommand(const FString& Command, const FString& PayloadJson)
{
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Web UI command received: %s"), *Command);
	if (Command == TEXT("hud_ready") || Command == TEXT("skill_panel_ready"))
	{
		bWebHUDReady = true;
		SendInitialWebHUDState();
		if (OverlayWidgetController) OverlayWidgetController->BroadcastAbilityInfo();
		if (AttributeMenuWidgetController) AttributeMenuWidgetController->BroadcastInitialValues();
		if (SpellMenuWidgetController) SpellMenuWidgetController->BroadcastInitialValues();
		return;
	}
	if (Command == TEXT("hud_attributes_clicked")) { SetWebHUDMenuLayout(true); SendAttributeCatalogToWebUI(); return; }
	if (Command == TEXT("hud_spells_clicked")) { SetWebHUDMenuLayout(true); SendSpellCatalogToWebUI(); return; }
	if (Command == TEXT("hud_menu_closed")) { SetWebHUDMenuLayout(false); return; }
	if (Command == TEXT("hud_close_clicked"))
	{
		SetWebHUDMenuLayout(true);
		if (WebUIBridge)
		{
			const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
			Payload->SetBoolField(TEXT("visible"), true);
			WebUIBridge->SendEvent(TEXT("hud_quit_prompt"), AuraHUDPrivate::SerializeObject(Payload));
		}
		return;
	}
	if (Command == TEXT("hud_quit_cancel"))
	{
		SetWebHUDMenuLayout(false);
		if (WebUIBridge)
		{
			const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
			Payload->SetBoolField(TEXT("visible"), false);
			WebUIBridge->SendEvent(TEXT("hud_quit_prompt"), AuraHUDPrivate::SerializeObject(Payload));
		}
		return;
	}
	if (Command == TEXT("hud_quit_confirm")) { UGameplayStatics::OpenLevel(this, FName(TEXT("LoadMenu"))); return; }
	if (Command == TEXT("hud_location_toggle")) { ToggleLocationDisplay(); return; }
	if (Command == TEXT("hud_reload"))
	{
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController())) AuraPC->RequestFirearmReload();
		return;
	}
	if (Command == TEXT("hud_inventory_clicked")) { SetWebHUDMenuLayout(true); SendEconomyStateToWebUI(); return; }
	if (Command == TEXT("hud_merchant_close"))
	{
		bMerchantUIOpen = false;
		SendMerchantToWebUI(WebMerchantComponent.Get(), false);
		SetWebHUDMenuLayout(false);
		return;
	}
	if (Command == TEXT("hud_merchant_open"))
	{
		AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController());
		AAuraCivilian* Civilian = AuraPC ? Cast<AAuraCivilian>(AuraPC->GetFocusedTargetActor()) : nullptr;
		UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		const bool bTradeAvailable = AuraPC
			&& AuraPC->GetFocusedTargetDescriptor().InteractionOptions.ContainsByPredicate(
				[](const FAuraInteractionOption& Option) { return Option.OptionTag.MatchesTagExact(FAuraGameplayTags::Get().Interaction_Trade) && Option.bEnabled; });
		if (Merchant && Merchant->IsMerchantActive() && bTradeAvailable)
		{
			bMerchantUIOpen = true;
			WebMerchantComponent = Merchant;
			SetWebHUDMenuLayout(true);
			SendMerchantToWebUI(Merchant, true);
		}
		return;
	}

	TSharedPtr<FJsonObject> Payload;
	if (!ParseWebPayload(PayloadJson, Payload))
	{
		if (Command.StartsWith(TEXT("hud_")) || Command.StartsWith(TEXT("skill_")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Ignoring malformed WebUI payload for %s"), *Command);
		}
		return;
	}

	if (Command == TEXT("hud_attribute_upgrade"))
	{
		FString AttributeTagName;
		if (Payload->TryGetStringField(TEXT("attributeTag"), AttributeTagName))
		{
			const FGameplayTag AttributeTag = FGameplayTag::RequestGameplayTag(FName(*AttributeTagName), false);
			if (IsKnownAttributeTag(AttributeTag) && AttributeMenuWidgetController) AttributeMenuWidgetController->UpgradeAttribute(AttributeTag);
		}
		return;
	}
	if (Command == TEXT("hud_spell_select"))
	{
		FString AbilityTagName;
		if (Payload->TryGetStringField(TEXT("abilityTag"), AbilityTagName))
		{
			const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName(*AbilityTagName), false);
			if (IsKnownAbilityTag(AbilityTag) && SpellMenuWidgetController) SpellMenuWidgetController->SpellGlobeSelected(AbilityTag);
		}
		return;
	}
	if (Command == TEXT("hud_spell_spend")) { if (SpellMenuWidgetController) SpellMenuWidgetController->SpendPointButtonPressed(); return; }
	if (Command == TEXT("hud_spell_equip")) { if (SpellMenuWidgetController) SpellMenuWidgetController->EquipButtonPressed(); return; }
	if (Command == TEXT("hud_spell_deselect")) { if (SpellMenuWidgetController) SpellMenuWidgetController->GlobeDeselect(); return; }
	if (Command == TEXT("hud_spell_slot"))
	{
		FString SlotTagName;
		FString AbilityTypeName;
		if (Payload->TryGetStringField(TEXT("slotTag"), SlotTagName) && Payload->TryGetStringField(TEXT("abilityType"), AbilityTypeName))
		{
			const FGameplayTag SlotTag = FGameplayTag::RequestGameplayTag(FName(*SlotTagName), false);
			const FGameplayTag AbilityType = FGameplayTag::RequestGameplayTag(FName(*AbilityTypeName), false);
			if (IsKnownSpellSlot(SlotTag) && AbilityType.IsValid() && SpellMenuWidgetController) SpellMenuWidgetController->SpellRowGlobePressed(SlotTag, AbilityType);
		}
		return;
	}
	if (Command == TEXT("hud_interaction_select"))
	{
		double Index = -1.0;
		if (Payload->TryGetNumberField(TEXT("index"), Index))
		{
			if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
			{
				AuraPC->SetFocusedInteractionOptionIndex(FMath::TruncToInt(Index));
			}
		}
		return;
	}
	if (Command == TEXT("hud_interaction_activate"))
	{
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController())) AuraPC->WebInteractPressed();
		return;
	}
	if (Command == TEXT("hud_merchant_buy"))
	{
		FString OfferId;
		if (!Payload->TryGetStringField(TEXT("offerId"), OfferId)) return;
		AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController());
		AAuraCivilian* Civilian = AuraPC ? Cast<AAuraCivilian>(AuraPC->GetFocusedTargetActor()) : nullptr;
		UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		if (AuraPC && AuraPC->GetInteractionComponent() && Merchant && Merchant->IsMerchantActive() && bMerchantUIOpen)
		{
			AuraPC->GetInteractionComponent()->RequestPurchase(Merchant->GetOwner(), FName(*OfferId));
		}
		return;
	}
	if (Command != TEXT("skill_ability_pressed") && Command != TEXT("skill_ability_held") && Command != TEXT("skill_ability_released")) return;

	FString InputTagName;
	if (!Payload->TryGetStringField(TEXT("inputTag"), InputTagName)) return;
	FGameplayTag InputTag;
	if (!TryGetWebAbilityInputTag(InputTagName, InputTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Rejected unsupported WebUI input tag '%s'"), *InputTagName);
		return;
	}
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		if (Command == TEXT("skill_ability_pressed"))
		{
			if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB)) bWebGameplayLMBDown = true;
			AuraPC->WebAbilityInputTagPressed(InputTag);
		}
		else if (Command == TEXT("skill_ability_held"))
		{
			AuraPC->WebAbilityInputTagHeld(InputTag);
		}
		else
		{
			if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB)) bWebGameplayLMBDown = false;
			AuraPC->WebAbilityInputTagReleased(InputTag);
		}
	}
}

void AAuraHUD::HandleWebUIConnectionChanged(bool bConnected)
{
	if (!bConnected && bWebGameplayLMBDown)
	{
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
		{
			AuraPC->WebAbilityInputTagReleased(FAuraGameplayTags::Get().InputTag_LMB);
		}
		bWebGameplayLMBDown = false;
	}
	if (!bConnected)
	{
		LastInteractionPayloadJson.Empty();
		bWebInteractionVisible = false;
		LastRoleStatePayloadJson.Empty();
		LastBattleStatePayloadJson.Empty();
		LastMerchantPayloadJson.Empty();
	}
	bWebHUDReady = false;
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] WebUI HUD %s; no native HUD fallback is enabled"), bConnected ? TEXT("connected") : TEXT("disconnected"));
	// A map travel can finish before the browser handshake. Replay the complete
	// authoritative snapshot when the first HUD page connects so its default
	// placeholders cannot survive a late role/pawn replication.
	if (bConnected)
	{
		LastRoleStatePayloadJson.Empty();
		LastBattleStatePayloadJson.Empty();
		SendInitialWebHUDState();
	}
}

void AAuraHUD::HandleHealthChangedForWebUI(float NewValue) { WebHealth = NewValue; SendVitalsToWebUI(); }
void AAuraHUD::HandleMaxHealthChangedForWebUI(float NewValue) { WebMaxHealth = NewValue; SendVitalsToWebUI(); }
void AAuraHUD::HandleManaChangedForWebUI(float NewValue) { WebMana = NewValue; SendVitalsToWebUI(); }
void AAuraHUD::HandleMaxManaChangedForWebUI(float NewValue) { WebMaxMana = NewValue; SendVitalsToWebUI(); }

void AAuraHUD::SendVitalsToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("health"), WebHealth);
	Payload->SetNumberField(TEXT("maxHealth"), WebMaxHealth);
	Payload->SetNumberField(TEXT("mana"), WebMana);
	Payload->SetNumberField(TEXT("maxMana"), WebMaxMana);
	WebUIBridge->SendEvent(TEXT("hud_vitals"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::SendPlayerProgressToWebUI()
{
	AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (PS)
	{
		WebPlayerLevel = PS->GetPlayerLevel();
		WebAttributePoints = PS->GetAttributePoints();
		WebSpellPoints = PS->GetSpellPoints();
		if (PS->LevelUpInfo)
		{
			const int32 Level = PS->LevelUpInfo->FindLevelForXP(PS->GetXP());
			if (Level > 0 && Level < PS->LevelUpInfo->LevelUpInformation.Num())
			{
				const int32 CurrentRequirement = PS->LevelUpInfo->LevelUpInformation[Level].LevelUpRequirement;
				const int32 PreviousRequirement = PS->LevelUpInfo->LevelUpInformation[Level - 1].LevelUpRequirement;
				const int32 Delta = CurrentRequirement - PreviousRequirement;
				WebXPPercent = Delta > 0 ? FMath::Clamp(static_cast<float>(PS->GetXP() - PreviousRequirement) / Delta, 0.f, 1.f) : 0.f;
			}
		}
	}
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("level"), WebPlayerLevel);
	Payload->SetNumberField(TEXT("xpPercent"), WebXPPercent);
	Payload->SetNumberField(TEXT("attributePoints"), WebAttributePoints);
	Payload->SetNumberField(TEXT("spellPoints"), WebSpellPoints);
	WebUIBridge->SendEvent(TEXT("hud_progress"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::HandleFirearmStateForWebUI(const FAuraFirearmState& State)
{
	SendFirearmStateToWebUI();
}

void AAuraHUD::HandleTutorialProgressForWebUI(uint32 CompletionMask)
{
	SendTutorialStateToWebUI();
}

void AAuraHUD::SendFirearmStateToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	const AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (!PS) return;
	const UAuraCombatStateComponent* Life = UAuraCombatStateComponent::FindForActor(PC->GetPawn());
	WebUIBridge->SendEvent(TEXT("hud_firearm"), AuraHUDPrivate::SerializeObject(
		BuildFirearmStatePayload(PS->GetFirearmState(), PS->GetRole(), Life && Life->IsAlive())));
}

TSharedRef<FJsonObject> AAuraHUD::BuildFirearmStatePayload(const FAuraFirearmState& State, FName RoleId, bool bAlive)
{
	const bool bApplicable = State.bApplicable && RoleId == TEXT("BungeeMan");
	const bool bCanReload = bApplicable && bAlive && !State.bReloading
		&& State.MagazineRounds < State.MagazineCapacity && State.ReserveRounds > 0;
	const TCHAR* DisplayState = !bApplicable ? TEXT("NotApplicable") : !bAlive ? TEXT("Unavailable")
		: State.bReloading ? TEXT("Reloading") : State.MagazineRounds > 0 ? TEXT("Ready")
		: State.ReserveRounds > 0 ? TEXT("EmptyMagazine") : TEXT("OutOfAmmo");
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("applicable"), bApplicable);
	Payload->SetStringField(TEXT("roleId"), RoleId.ToString());
	Payload->SetStringField(TEXT("abilityTag"), bApplicable ? TEXT("Abilities.Gun.Fire") : TEXT(""));
	Payload->SetStringField(TEXT("inputTag"), bApplicable ? TEXT("InputTag.LMB") : TEXT(""));
	Payload->SetStringField(TEXT("state"), DisplayState);
	Payload->SetNumberField(TEXT("magazineRounds"), State.MagazineRounds);
	Payload->SetNumberField(TEXT("magazineCapacity"), State.MagazineCapacity);
	Payload->SetNumberField(TEXT("reserveRounds"), State.ReserveRounds);
	Payload->SetNumberField(TEXT("reserveCapacity"), State.ReserveCapacity);
	Payload->SetNumberField(TEXT("ammoRevision"), State.AmmoRevision);
	Payload->SetNumberField(TEXT("reloadSerial"), State.ReloadSerial);
	Payload->SetNumberField(TEXT("reloadDuration"), State.ReloadDuration);
	Payload->SetStringField(TEXT("fireMode"), State.FireMode.ToString());
	Payload->SetBoolField(TEXT("canFire"), bApplicable && bAlive && !State.bReloading && State.MagazineRounds > 0);
	Payload->SetBoolField(TEXT("canReload"), bCanReload);
	Payload->SetStringField(TEXT("unavailableReason"), !bApplicable ? TEXT("Not applicable to Aura") : !bAlive ? TEXT("Cannot fire or reload while recovering")
		: State.bReloading ? TEXT("Reloading - wait for completion") : State.MagazineRounds <= 0
		? (State.ReserveRounds > 0 ? TEXT("Magazine empty - press R to reload") : TEXT("Out of ammo - no reserve rounds")) : TEXT(""));
	return Payload;
}

void AAuraHUD::SendTutorialStateToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	const AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	if (!PS) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("completionMask"), PS->GetTutorialCompletionMask());
	Payload->SetStringField(TEXT("recoveryState"), PS->GetRecoveryState().ToString());
	Payload->SetStringField(TEXT("authority"), TEXT("server"));
	WebUIBridge->SendEvent(TEXT("hud_tutorial"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::SendRoleStateToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	AAuraCharacterBase* Character = PC ? Cast<AAuraCharacterBase>(PC->GetPawn()) : nullptr;
	// PlayerState role replication is authoritative and can arrive one frame
	// before the pawn's AppliedRoleState. Prefer the applied pawn state when it
	// is valid, but fall back to the replicated PlayerState role so the HUD does
	// not remain on "Loading role" during that short travel/replication window.
	const FName AppliedRoleId = Character ? Character->GetAppliedRoleState().RoleId : NAME_None;
	const FName DisplayRoleId = !AppliedRoleId.IsNone() ? AppliedRoleId : (PS ? PS->GetRole() : NAME_None);
	if (Character)
	{
		const FAuraAppliedRoleState& RoleState = Character->GetAppliedRoleState();
		const FAuraCombatIdentity& Identity = Character->GetCombatIdentity();
		Payload->SetStringField(TEXT("roleId"), DisplayRoleId.ToString());
		AuraHUDPrivate::SetTagField(Payload, TEXT("entityType"), RoleState.EntityTypeTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("economyProfile"), RoleState.EconomyProfileTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("interactionProfile"), RoleState.InteractionProfileTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("faction"), Identity.FactionTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("controlType"), Identity.ControlTypeTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("combatProfile"), Identity.CombatProfileTag);
		AuraHUDPrivate::SetTagField(Payload, TEXT("deathPolicy"), Identity.DeathPolicyTag);
		Payload->SetBoolField(TEXT("targetable"), Identity.bTargetable);
		Payload->SetBoolField(TEXT("canAttack"), Identity.bCanAttack);
		Payload->SetBoolField(TEXT("canBeDamaged"), Identity.bCanBeDamaged);
		Payload->SetBoolField(TEXT("allowFriendlyFire"), Identity.bAllowFriendlyFire);
		Payload->SetBoolField(TEXT("identityValid"), Character->HasValidCombatIdentity());

		const EAuraCombatLifeState LifeState = Character->GetCombatLifeState();
		Payload->SetNumberField(TEXT("lifeState"), static_cast<int32>(LifeState));
		Payload->SetStringField(TEXT("lifeStateName"), AuraHUDPrivate::EnumName(LifeState));
		if (LifeState != LastWebLifeState && WebUIBridge->IsServerRunning())
		{
			const TSharedRef<FJsonObject> LifePayload = MakeShared<FJsonObject>();
			LifePayload->SetStringField(TEXT("message"), FString::Printf(TEXT("Life state: %s"), *AuraHUDPrivate::EnumName(LifeState)));
			WebUIBridge->SendEvent(TEXT("hud_message"), AuraHUDPrivate::SerializeObject(LifePayload));
			LastWebLifeState = LifeState;
		}
	}
	else
	{
		Payload->SetStringField(TEXT("lifeStateName"), TEXT("Unknown"));
		Payload->SetBoolField(TEXT("identityValid"), false);
	}

	if (PS)
	{
		Payload->SetStringField(TEXT("roleId"), DisplayRoleId.ToString());
		Payload->SetNumberField(TEXT("economyState"), static_cast<int32>(PS->GetEconomyInitializationState()));
		Payload->SetStringField(TEXT("economyStateName"), AuraHUDPrivate::EnumName(PS->GetEconomyInitializationState()));
		Payload->SetBoolField(TEXT("persistentProfile"), PS->GetEconomyInitializationState() == EAuraEconomyInitializationState::LoadedPersistent);
		Payload->SetNumberField(TEXT("tutorialCompletionMask"), PS->GetTutorialCompletionMask());
		Payload->SetStringField(TEXT("recoveryState"), PS->GetRecoveryState().ToString());
	}

	const FString PayloadJson = AuraHUDPrivate::SerializeObject(Payload);
	if (PayloadJson == LastRoleStatePayloadJson) return;
	LastRoleStatePayloadJson = PayloadJson;
	WebUIBridge->SendEvent(TEXT("hud_role_state"), PayloadJson);
	// Life/role transitions must also refresh firearm availability, even without an ammo change.
	SendFirearmStateToWebUI();
}

void AAuraHUD::SendBattleStateToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("phaseName"), TEXT("Unknown"));
	Payload->SetNumberField(TEXT("phase"), static_cast<int32>(EAuraBattlePhase::Peace));
	Payload->SetStringField(TEXT("eventId"), FString());
	Payload->SetNumberField(TEXT("populationActive"), 0);
	Payload->SetNumberField(TEXT("populationMaximum"), 0);
	Payload->SetNumberField(TEXT("populationPending"), 0);
	Payload->SetNumberField(TEXT("populationCasualties"), 0);
	Payload->SetBoolField(TEXT("available"), false);
	for (TActorIterator<AAuraBattleDirector> It(GetWorld()); It; ++It)
	{
		const AAuraBattleDirector* Director = *It;
		if (!Director) continue;
		const EAuraBattlePhase Phase = Director->GetCurrentPhase();
		Payload->SetNumberField(TEXT("phase"), static_cast<int32>(Phase));
		Payload->SetStringField(TEXT("phaseName"), AuraHUDPrivate::EnumName(Phase));
		Payload->SetStringField(TEXT("eventId"), Director->GetActiveBattleEventId().ToString());
		Payload->SetNumberField(TEXT("configVersion"), Director->GetConfigVersion());
		Payload->SetNumberField(TEXT("populationActive"), Director->GetPopulationActiveCount());
		Payload->SetNumberField(TEXT("populationMaximum"), Director->GetPopulationMaximumCount());
		Payload->SetNumberField(TEXT("populationPending"), Director->GetPopulationPendingCount());
		Payload->SetNumberField(TEXT("populationCasualties"), Director->GetPopulationCasualtyCount());
		Payload->SetBoolField(TEXT("available"), true);
		break;
	}

	const FString PayloadJson = AuraHUDPrivate::SerializeObject(Payload);
	if (PayloadJson == LastBattleStatePayloadJson) return;
	LastBattleStatePayloadJson = PayloadJson;
	WebUIBridge->SendEvent(TEXT("hud_battle_state"), PayloadJson);
}

void AAuraHUD::SendEconomyStateToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	const UAuraCurrencyComponent* Currency = PS ? PS->GetCurrencyComponent() : nullptr;
	const UAuraInventoryComponent* Inventory = PS ? PS->GetInventoryComponent() : nullptr;
	Payload->SetStringField(TEXT("currencyId"), Currency ? Currency->GetCurrencyId().ToString() : FString());
	Payload->SetNumberField(TEXT("balance"), Currency ? static_cast<double>(Currency->GetBalance()) : 0.0);
	Payload->SetNumberField(TEXT("walletRevision"), Currency ? Currency->GetRevision() : 0);
	Payload->SetNumberField(TEXT("inventoryRevision"), Inventory ? Inventory->GetRevision() : 0);
	Payload->SetNumberField(TEXT("usedSlots"), Inventory ? Inventory->GetUsedSlotCount() : 0);
	Payload->SetStringField(TEXT("initializationState"), PS ? AuraHUDPrivate::EnumName(PS->GetEconomyInitializationState()) : FString(TEXT("Unknown")));

	int32 MaxSlots = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UAuraEconomyRegistrySubsystem* Registry = GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>())
			{
				if (Registry->IsReady()) MaxSlots = static_cast<int32>(Registry->GetSnapshot()->Settings.MaximumItemSlots);
			}
		}
	}
	Payload->SetNumberField(TEXT("maxSlots"), MaxSlots);

	TArray<TSharedPtr<FJsonValue>> Items;
	if (Inventory)
	{
		const UAuraEconomyRegistrySubsystem* Registry = nullptr;
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance()) Registry = GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>();
		}
		for (const FAuraInventorySlot& Slot : Inventory->GetSlots())
		{
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("itemId"), Slot.ItemId.ToString());
			Item->SetNumberField(TEXT("quantity"), static_cast<double>(Slot.Quantity));
			if (Registry)
			{
				if (const FAuraItemDefinition* Definition = Registry->FindItem(Slot.ItemId)) Item->SetStringField(TEXT("displayName"), Definition->DisplayName.ToString());
			}
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
	}
	Payload->SetArrayField(TEXT("items"), Items);
	WebUIBridge->SendEvent(TEXT("hud_economy"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::SendMerchantToWebUI(UAuraMerchantComponent* MerchantComponent, bool bVisible)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("visible"), bVisible && MerchantComponent && MerchantComponent->IsMerchantActive());
	if (MerchantComponent)
	{
		const FAuraMerchantPresentation& Presentation = MerchantComponent->GetPresentation();
		Payload->SetStringField(TEXT("populationMemberId"), Presentation.PopulationMemberId.ToString());
		Payload->SetStringField(TEXT("merchantDefinitionId"), Presentation.MerchantDefinitionId.ToString());
		Payload->SetBoolField(TEXT("available"), Presentation.bAvailable);
		Payload->SetNumberField(TEXT("stockRevision"), Presentation.StockRevision);
		TArray<TSharedPtr<FJsonValue>> Offers;
		for (const FAuraMerchantOfferPresentation& Offer : Presentation.Offers)
		{
			const TSharedRef<FJsonObject> OfferObject = MakeShared<FJsonObject>();
			OfferObject->SetStringField(TEXT("offerId"), Offer.OfferId.ToString());
			OfferObject->SetStringField(TEXT("itemId"), Offer.ItemId.ToString());
			OfferObject->SetStringField(TEXT("itemDisplayName"), Offer.ItemDisplayName.ToString());
			OfferObject->SetNumberField(TEXT("grantQuantity"), static_cast<double>(Offer.GrantQuantity));
			OfferObject->SetNumberField(TEXT("buyPrice"), static_cast<double>(Offer.BuyPrice));
			OfferObject->SetNumberField(TEXT("stockPolicy"), static_cast<int32>(Offer.StockPolicy));
			OfferObject->SetStringField(TEXT("stockPolicyName"), AuraHUDPrivate::EnumName(Offer.StockPolicy));
			OfferObject->SetNumberField(TEXT("currentStock"), static_cast<double>(Offer.CurrentStock));
			OfferObject->SetNumberField(TEXT("stockRevision"), Offer.StockRevision);
			OfferObject->SetBoolField(TEXT("available"), Offer.bAvailable);
			Offers.Add(MakeShared<FJsonValueObject>(OfferObject));
		}
		Payload->SetArrayField(TEXT("offers"), Offers);
	}

	const FString PayloadJson = AuraHUDPrivate::SerializeObject(Payload);
	if (PayloadJson == LastMerchantPayloadJson) return;
	LastMerchantPayloadJson = PayloadJson;
	WebUIBridge->SendEvent(TEXT("hud_merchant"), PayloadJson);
}

void AAuraHUD::SendMerchantClearedToWebUI()
{
	bMerchantUIOpen = false;
	WebMerchantComponent = nullptr;
	LastMerchantPayloadJson.Empty();
	if (WebUIBridge && WebUIBridge->IsServerRunning()) WebUIBridge->SendEvent(TEXT("hud_merchant_cleared"), TEXT("{}"));
}

void AAuraHUD::SendAttributeCatalogToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	// Attribute rows stream individually from the controller; this marker lets a
	// newly opened WebUI panel clear stale rows before the replay arrives.
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("replace"), true);
	WebUIBridge->SendEvent(TEXT("hud_attribute_catalog"), AuraHUDPrivate::SerializeObject(Payload));
	if (AttributeMenuWidgetController) AttributeMenuWidgetController->BroadcastInitialValues();
}

void AAuraHUD::SendSpellCatalogToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	AAuraPlayerState* PS = PC ? PC->GetPlayerState<AAuraPlayerState>() : nullptr;
	UAuraAbilitySystemComponent* ASC = PS ? Cast<UAuraAbilitySystemComponent>(PS->GetAbilitySystemComponent()) : nullptr;
	if (!ASC) return;

	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Abilities;
	if (URuntimeAbilityInfo* RuntimeInfo = UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(PC))
	{
		for (const FAuraAbilityInfo& Metadata : RuntimeInfo->GetAllAbilityInfo())
		{
			if (!Metadata.AbilityTag.IsValid()) continue;
			const FAuraAbilityInfo Info = ASC->GetRuntimeAbilityInfoForTag(Metadata.AbilityTag);
			const FGameplayTag Status = ASC->GetStatusFromAbilityTag(Metadata.AbilityTag);
			const FGameplayTag Slot = ASC->GetSlotFromAbilityTag(Metadata.AbilityTag);
			FString Description;
			FString NextDescription;
			ASC->GetDescriptionsByAbilityTag(Metadata.AbilityTag, Description, NextDescription);
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			AuraHUDPrivate::SetTagField(Entry, TEXT("abilityTag"), Metadata.AbilityTag);
			AuraHUDPrivate::SetTagField(Entry, TEXT("abilityType"), Info.AbilityType);
			AuraHUDPrivate::SetTagField(Entry, TEXT("statusTag"), Status);
			AuraHUDPrivate::SetTagField(Entry, TEXT("slotTag"), Slot);
			Entry->SetNumberField(TEXT("levelRequirement"), Metadata.LevelRequirement);
			Entry->SetStringField(TEXT("description"), Description);
			Entry->SetStringField(TEXT("nextDescription"), NextDescription);
			const FString ManualIconDataUri = BuildManualSkillIconDataUri(Metadata.AbilityTag);
			Entry->SetStringField(TEXT("icon"), ManualIconDataUri.IsEmpty() ? BuildAbilityIconDataUri(Metadata.Icon) : ManualIconDataUri);
			Entry->SetStringField(TEXT("iconName"), IsValid(Metadata.Icon) ? Metadata.Icon->GetPathName() : FString());
			Entry->SetNumberField(TEXT("level"), 0);
			if (const FGameplayAbilitySpec* Spec = ASC->GetSpecFromAbilityTag(Metadata.AbilityTag)) Entry->SetNumberField(TEXT("level"), Spec->Level);
			Abilities.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}
	Payload->SetArrayField(TEXT("abilities"), Abilities);
	Payload->SetNumberField(TEXT("spellPoints"), WebSpellPoints);
	WebUIBridge->SendEvent(TEXT("hud_spell_catalog"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::HandleAbilityInfoForWebUI(const FAuraAbilityInfo& Info)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning() || !Info.InputTag.IsValid()) return;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("inputTag"), Info.InputTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("abilityTag"), Info.AbilityTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("statusTag"), Info.StatusTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("abilityType"), Info.AbilityType);
	Payload->SetNumberField(TEXT("levelRequirement"), Info.LevelRequirement);
	const bool bClear = Info.AbilityTag.MatchesTagExact(GameplayTags.Abilities_None);
	Payload->SetBoolField(TEXT("clear"), bClear);
	const FString ManualIconDataUri = BuildManualSkillIconDataUri(Info.AbilityTag);
	Payload->SetStringField(TEXT("icon"), ManualIconDataUri.IsEmpty() ? (bClear ? FString() : BuildAbilityIconDataUri(Info.Icon)) : ManualIconDataUri);
	Payload->SetStringField(TEXT("iconName"), IsValid(Info.Icon) ? Info.Icon->GetPathName() : FString());
	WebUIBridge->SendEvent(TEXT("skill_panel_ability"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::HandleMessageForWebUI(FUIWidgetRow Row)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("messageTag"), Row.MessageTag);
	Payload->SetStringField(TEXT("message"), Row.Message.ToString());
	Payload->SetStringField(TEXT("icon"), BuildAbilityIconDataUri(Row.Image));
	WebUIBridge->SendEvent(TEXT("hud_message"), AuraHUDPrivate::SerializeObject(Payload));
}

void AAuraHUD::HandleXPPercentChangedForWebUI(float NewValue) { WebXPPercent = NewValue; SendPlayerProgressToWebUI(); }
void AAuraHUD::HandlePlayerLevelChangedForWebUI(int32 NewLevel, bool bLevelUp)
{
	WebPlayerLevel = NewLevel;
	SendPlayerProgressToWebUI();
	if (bLevelUp && WebUIBridge && WebUIBridge->IsServerRunning())
	{
		const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetNumberField(TEXT("level"), NewLevel);
		WebUIBridge->SendEvent(TEXT("hud_level_up"), AuraHUDPrivate::SerializeObject(Payload));
	}
}
void AAuraHUD::HandleMountedChangedForWebUI(bool bIsMounted)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("mounted"), bIsMounted);
	WebUIBridge->SendEvent(TEXT("hud_mounted"), AuraHUDPrivate::SerializeObject(Payload));
}
void AAuraHUD::HandleAttributeInfoForWebUI(const FAuraAttributeInfo& Info)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("attributeTag"), Info.AttributeTag);
	Payload->SetStringField(TEXT("name"), Info.AttributeName.ToString());
	Payload->SetStringField(TEXT("description"), Info.AttributeDescription.ToString());
	Payload->SetNumberField(TEXT("value"), Info.AttributeValue);
	WebUIBridge->SendEvent(TEXT("hud_attribute"), AuraHUDPrivate::SerializeObject(Payload));
}
void AAuraHUD::HandleAttributePointsChangedForWebUI(int32 NewValue) { WebAttributePoints = NewValue; SendPlayerProgressToWebUI(); }
void AAuraHUD::HandleSpellPointsChangedForWebUI(int32 NewValue) { WebSpellPoints = NewValue; SendPlayerProgressToWebUI(); }
void AAuraHUD::HandleSpellSelectionForWebUI(bool bSpendPointsButtonEnabled, bool bEquipButtonEnabled, FString DescriptionString, FString NextLevelDescriptionString)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("canSpend"), bSpendPointsButtonEnabled);
	Payload->SetBoolField(TEXT("canEquip"), bEquipButtonEnabled);
	Payload->SetStringField(TEXT("description"), DescriptionString);
	Payload->SetStringField(TEXT("nextDescription"), NextLevelDescriptionString);
	WebUIBridge->SendEvent(TEXT("hud_spell_selection"), AuraHUDPrivate::SerializeObject(Payload));
}
void AAuraHUD::HandleWaitForEquipForWebUI(const FGameplayTag& AbilityType)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("abilityType"), AbilityType);
	WebUIBridge->SendEvent(TEXT("hud_wait_for_equip"), AuraHUDPrivate::SerializeObject(Payload));
}
void AAuraHUD::HandleStopWaitingForEquipForWebUI(const FGameplayTag& AbilityType)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("abilityType"), AbilityType);
	WebUIBridge->SendEvent(TEXT("hud_stop_wait_for_equip"), AuraHUDPrivate::SerializeObject(Payload));
	SendSpellCatalogToWebUI();
}
void AAuraHUD::HandleSpellReassignedForWebUI(const FGameplayTag& AbilityTag)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("abilityTag"), AbilityTag);
	WebUIBridge->SendEvent(TEXT("hud_spell_reassigned"), AuraHUDPrivate::SerializeObject(Payload));
	SendSpellCatalogToWebUI();
}

void AAuraHUD::SendInteractionToWebUI(const FAuraTargetDescriptor& Descriptor)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	bWebInteractionVisible = true;
	SetWebHUDInteractionLayout(true);
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("visible"), true);
	AuraHUDPrivate::SetTagField(Payload, TEXT("relationshipTag"), Descriptor.RelationshipTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("kindTag"), Descriptor.KindTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("lifeTag"), Descriptor.LifeTag);
	Payload->SetStringField(TEXT("displayName"), Descriptor.DisplayName.ToString());
	Payload->SetNumberField(TEXT("health"), Descriptor.Health);
	Payload->SetNumberField(TEXT("maxHealth"), Descriptor.MaxHealth);
	Payload->SetBoolField(TEXT("attackAllowed"), Descriptor.bLocallyAttackAllowed);
	AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	AAuraCivilian* Civilian = AuraPC ? Cast<AAuraCivilian>(AuraPC->GetFocusedTargetActor()) : nullptr;
	UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
	if (Civilian)
	{
		const FAuraPopulationMemberState& MemberState = Civilian->GetPopulationMemberState();
		Payload->SetStringField(TEXT("populationMemberId"), MemberState.PopulationMemberId.ToString());
		Payload->SetStringField(TEXT("workProfileId"), MemberState.WorkProfileId.ToString());
		Payload->SetStringField(TEXT("zoneId"), MemberState.ZoneId.ToString());
		Payload->SetStringField(TEXT("activity"), AuraHUDPrivate::EnumName(Civilian->GetCivilianActivity()));
		Payload->SetBoolField(TEXT("merchantAvailable"), Merchant && Merchant->IsMerchantActive());
	}
	TArray<TSharedPtr<FJsonValue>> Options;
	for (int32 Index = 0; Index < Descriptor.InteractionOptions.Num(); ++Index)
	{
		const FAuraInteractionOption& Option = Descriptor.InteractionOptions[Index];
		const TSharedRef<FJsonObject> OptionObject = MakeShared<FJsonObject>();
		AuraHUDPrivate::SetTagField(OptionObject, TEXT("optionTag"), Option.OptionTag);
		OptionObject->SetStringField(TEXT("displayText"), Option.DisplayText.ToString());
		OptionObject->SetBoolField(TEXT("enabled"), Option.bEnabled);
		OptionObject->SetNumberField(TEXT("disabledReason"), static_cast<int32>(Option.DisabledReason));
		OptionObject->SetNumberField(TEXT("index"), Index);
		Options.Add(MakeShared<FJsonValueObject>(OptionObject));
	}
	Payload->SetArrayField(TEXT("options"), Options);
	const FString PayloadJson = AuraHUDPrivate::SerializeObject(Payload);
	if (PayloadJson != LastInteractionPayloadJson)
	{
		LastInteractionPayloadJson = PayloadJson;
		WebUIBridge->SendEvent(TEXT("hud_interaction"), PayloadJson);
	}
	if (Merchant)
	{
		WebMerchantComponent = Merchant;
		SendMerchantToWebUI(Merchant, bMerchantUIOpen);
	}
	else if (bMerchantUIOpen || WebMerchantComponent.IsValid())
	{
		SendMerchantClearedToWebUI();
	}
}
void AAuraHUD::HandleTargetPreviewForWebUI(const FAuraTargetDescriptor& Descriptor) { SendInteractionToWebUI(Descriptor); }
void AAuraHUD::HandleTargetPreviewClearedForWebUI()
{
	bWebInteractionVisible = false;
	LastInteractionPayloadJson.Empty();
	if (WebUIBridge && WebUIBridge->IsServerRunning()) WebUIBridge->SendEvent(TEXT("hud_interaction_cleared"), TEXT("{}"));
	if (bMerchantUIOpen || WebMerchantComponent.IsValid()) SendMerchantClearedToWebUI();
	SetWebHUDInteractionLayout(false);
}

void AAuraHUD::HandleCurrencyChangedForWebUI(int64 NewBalance, uint32 NewRevision)
{
	SendEconomyStateToWebUI();
}

void AAuraHUD::HandleInventoryChangedForWebUI(const TArray<FAuraInventorySlot>& Slots, uint32 NewRevision)
{
	SendEconomyStateToWebUI();
}

void AAuraHUD::HandlePurchaseResultForWebUI(const FGuid& SessionNonce, uint64 RequestId, EAuraCommerceResultCode ResultCode,
	uint32 WalletRevision, uint32 InventoryRevision, uint32 StockRevision)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("sessionNonce"), SessionNonce.ToString());
	Payload->SetNumberField(TEXT("requestId"), static_cast<double>(RequestId));
	Payload->SetNumberField(TEXT("resultCode"), static_cast<int32>(ResultCode));
	Payload->SetStringField(TEXT("resultName"), AuraHUDPrivate::EnumName(ResultCode));
	Payload->SetNumberField(TEXT("walletRevision"), WalletRevision);
	Payload->SetNumberField(TEXT("inventoryRevision"), InventoryRevision);
	Payload->SetNumberField(TEXT("stockRevision"), StockRevision);
	WebUIBridge->SendEvent(TEXT("hud_merchant_result"), AuraHUDPrivate::SerializeObject(Payload));
	SendEconomyStateToWebUI();
}

void AAuraHUD::HandleRoleChangedForWebUI(FName NewRole)
{
	LastRoleStatePayloadJson.Empty();
	SendRoleStateToWebUI();
}

void AAuraHUD::SendLocationToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning()) return;
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetBoolField(TEXT("visible"), bShowLocation);
	if (bShowLocation)
	{
		if (APawn* Pawn = GetOwningPlayerController() ? GetOwningPlayerController()->GetPawn() : nullptr)
		{
			const FVector Location = Pawn->GetActorLocation();
			Payload->SetNumberField(TEXT("x"), Location.X);
			Payload->SetNumberField(TEXT("y"), Location.Y);
			Payload->SetNumberField(TEXT("z"), Location.Z);
		}
	}
	const FString PayloadJson = AuraHUDPrivate::SerializeObject(Payload);
	if (PayloadJson == LastLocationPayloadJson) return;
	LastLocationPayloadJson = PayloadJson;
	WebUIBridge->SendEvent(TEXT("hud_location"), PayloadJson);
}

bool AAuraHUD::ParseWebPayload(const FString& PayloadJson, TSharedPtr<FJsonObject>& OutPayload) const
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	return FJsonSerializer::Deserialize(Reader, OutPayload) && OutPayload.IsValid();
}
bool AAuraHUD::IsKnownAbilityTag(const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid()) return false;
	if (URuntimeAbilityInfo* RuntimeInfo = UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(Cast<AAuraPlayerController>(GetOwningPlayerController())))
	{
		return RuntimeInfo->GetAllAbilityInfo().ContainsByPredicate([&AbilityTag](const FAuraAbilityInfo& Info) { return Info.AbilityTag.MatchesTagExact(AbilityTag); });
	}
	return false;
}
bool AAuraHUD::IsKnownAttributeTag(const FGameplayTag& AttributeTag) const
{
	if (!AttributeTag.IsValid()) return false;
	if (AAuraPlayerController* PC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		if (AAuraPlayerState* PS = PC->GetPlayerState<AAuraPlayerState>())
		{
			if (const UAuraAttributeSet* AS = Cast<UAuraAttributeSet>(PS->GetAttributeSet())) return AS->TagsToAttributes.Contains(AttributeTag);
		}
	}
	return false;
}
bool AAuraHUD::IsKnownSpellSlot(const FGameplayTag& SlotTag) const
{
	if (!SlotTag.IsValid()) return false;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	return SlotTag == GameplayTags.InputTag_LMB || SlotTag == GameplayTags.InputTag_RMB || SlotTag == GameplayTags.InputTag_1 || SlotTag == GameplayTags.InputTag_2
		|| SlotTag == GameplayTags.InputTag_3 || SlotTag == GameplayTags.InputTag_4 || SlotTag == GameplayTags.InputTag_Passive_1 || SlotTag == GameplayTags.InputTag_Passive_2;
}
bool AAuraHUD::TryGetWebAbilityInputTag(const FString& InputTagName, FGameplayTag& OutInputTag) const
{
	OutInputTag = FGameplayTag::RequestGameplayTag(FName(*InputTagName), false);
	return IsKnownSpellSlot(OutInputTag);
}

FString AAuraHUD::BuildAbilityIconDataUri(const UTexture2D* Icon)
{
	if (!IsValid(Icon)) return FString();
	const FString CacheKey = Icon->GetPathName();
	if (const FString* Cached = AbilityIconDataUriCache.Find(CacheKey)) return *Cached;
	FString DataUri;
	const FTexturePlatformData* PlatformData = Icon->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0) { AbilityIconDataUriCache.Add(CacheKey, DataUri); return DataUri; }
	const FTexture2DMipMap& Mip = PlatformData->Mips[0];
	const int32 Width = Mip.SizeX > 0 ? Mip.SizeX : PlatformData->SizeX;
	const int32 Height = Mip.SizeY > 0 ? Mip.SizeY : PlatformData->SizeY;
	const int64 ExpectedBytes = static_cast<int64>(Width) * static_cast<int64>(Height) * sizeof(FColor);
	if (Width <= 0 || Height <= 0 || ExpectedBytes <= 0 || Mip.BulkData.GetBulkDataSize() < ExpectedBytes) { AbilityIconDataUriCache.Add(CacheKey, DataUri); return DataUri; }
	const void* RawPixels = Mip.BulkData.LockReadOnly();
	if (!RawPixels) { AbilityIconDataUriCache.Add(CacheKey, DataUri); return DataUri; }
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), RawPixels, ExpectedBytes);
	Mip.BulkData.Unlock();
	TArray<uint8> PngBytes;
	FImageUtils::CompressImageArray(Width, Height, Pixels, PngBytes);
	if (PngBytes.Num() > 0) DataUri = FString::Printf(TEXT("data:image/png;base64,%s"), *FBase64::Encode(PngBytes));
	AbilityIconDataUriCache.Add(CacheKey, DataUri);
	return DataUri;
}

FString AAuraHUD::BuildManualSkillIconDataUri(const FGameplayTag& AbilityTag)
{
	const FString CacheKey = AbilityTag.ToString();
	if (const FString* Cached = ManualSkillIconDataUriCache.Find(CacheKey)) return *Cached;

	const TCHAR* IconFilename = AuraHUDPrivate::GetManualSkillIconFilename(AbilityTag);
	if (!IconFilename)
	{
		ManualSkillIconDataUriCache.Add(CacheKey, FString());
		return FString();
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AuraWebUI"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] AuraWebUI plugin was not found while loading manual skill icon %s"), *CacheKey);
		ManualSkillIconDataUriCache.Add(CacheKey, FString());
		return FString();
	}

	const FString IconPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/WebUI/skill-icons"), IconFilename);
	TArray<uint8> PngBytes;
	if (!FFileHelper::LoadFileToArray(PngBytes, *IconPath) || PngBytes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Manual WebUI skill icon is unavailable: %s"), *IconPath);
		ManualSkillIconDataUriCache.Add(CacheKey, FString());
		return FString();
	}

	const FString DataUri = FString::Printf(TEXT("data:image/png;base64,%s"), *FBase64::Encode(PngBytes));
	ManualSkillIconDataUriCache.Add(CacheKey, DataUri);
	return DataUri;
}

void AAuraHUD::ToggleLocationDisplay()
{
	bShowLocation = !bShowLocation;
	LastLocationPayloadJson.Empty();
	SendLocationToWebUI();
}
void AAuraHUD::DrawHUD()
{
	Super::DrawHUD();
	SendLocationToWebUI();
	SendRoleStateToWebUI();
	SendBattleStateToWebUI();
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		const FAuraTargetDescriptor& Descriptor = AuraPC->GetFocusedTargetDescriptor();
		if (Descriptor.IsValid()) SendInteractionToWebUI(Descriptor);
		else if (bWebInteractionVisible) HandleTargetPreviewClearedForWebUI();
	}
}
