// Copyright Druid Mechanics

#include "UI/HUD/AuraHUD.h"

#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
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
	}

	if (WebUIBridge)
	{
		WebUIBridge->OnCommand.RemoveDynamic(this, &AAuraHUD::HandleWebUICommand);
		WebUIBridge->OnConnectionChanged.RemoveDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);
	}

	if (WebHUD && IsValid(WebHUD)) WebHUD->RemoveFromParent();
	WebHUD = nullptr;
	WebUIBridge = nullptr;
	bWebHUDReady = false;
	AbilityIconDataUriCache.Empty();
	LastInteractionPayloadJson.Empty();
	LastLocationPayloadJson.Empty();

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
	if (!PC || !PC->IsLocalController() || WebHUD) return;

	UWorld* World = GetWorld();
	if (!World) return;
	WebUIBridge = World->GetSubsystem<UWebUIBridgeSubsystem>();
	if (!WebUIBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web HUD skipped: bridge subsystem unavailable"));
		return;
	}

	// Use the local-player context in gameplay so the browser shares the active
	// viewport. The world context keeps headless editor automation constructible.
	WebHUD = PC->GetLocalPlayer()
		? CreateWidget<UWebUIWidget>(PC, UWebUIWidget::StaticClass())
		: CreateWidget<UWebUIWidget>(World, UWebUIWidget::StaticClass());
	if (!WebHUD)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraHUD] Failed to create the full-screen WebUI HUD host"));
		WebUIBridge = nullptr;
		return;
	}

	WebHUD->HtmlAssetPath = TEXT("WebUI/hud.html");
	WebHUD->bAutoConnectBridge = false;
	WebHUD->ConfigureViewportLayout(FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f), FVector2D(0.f, 0.f));

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
	}

	WebUIBridge->OnCommand.AddDynamic(this, &AAuraHUD::HandleWebUICommand);
	WebUIBridge->OnConnectionChanged.AddDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);
	WebHUD->AddToViewport(200);
	WebHUD->ReloadWebUI();
	WebHUD->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Full-screen WebUI HUD mounted (context=%s owningPlayer=%s browser=%s)"),
		PC->GetLocalPlayer() ? TEXT("LocalPlayer") : TEXT("WorldFallback"),
		*GetNameSafe(WebHUD->GetOwningPlayer()),
		*GetNameSafe(WebHUD->GetWebBrowser()));
}

void AAuraHUD::SendInitialWebHUDState()
{
	if (!WebUIBridge) return;
	SendVitalsToWebUI();
	SendPlayerProgressToWebUI();
	SendAttributeCatalogToWebUI();
	SendSpellCatalogToWebUI();
	SendLocationToWebUI();
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
	if (Command == TEXT("hud_attributes_clicked")) { SendAttributeCatalogToWebUI(); return; }
	if (Command == TEXT("hud_spells_clicked")) { SendSpellCatalogToWebUI(); return; }
	if (Command == TEXT("hud_close_clicked"))
	{
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
			if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController())) AuraPC->SetFocusedInteractionOptionIndex(FMath::TruncToInt(Index));
		}
		return;
	}
	if (Command == TEXT("hud_interaction_activate"))
	{
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController())) AuraPC->WebInteractPressed();
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
	bWebHUDReady = false;
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] WebUI HUD %s; no native HUD fallback is enabled"), bConnected ? TEXT("connected") : TEXT("disconnected"));
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
			Entry->SetStringField(TEXT("icon"), BuildAbilityIconDataUri(Metadata.Icon));
			Entry->SetStringField(TEXT("iconName"), Metadata.Icon ? Metadata.Icon->GetPathName() : FString());
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
	Payload->SetStringField(TEXT("icon"), bClear ? FString() : BuildAbilityIconDataUri(Info.Icon));
	Payload->SetStringField(TEXT("iconName"), Info.Icon ? Info.Icon->GetPathName() : FString());
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
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	AuraHUDPrivate::SetTagField(Payload, TEXT("relationshipTag"), Descriptor.RelationshipTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("kindTag"), Descriptor.KindTag);
	AuraHUDPrivate::SetTagField(Payload, TEXT("lifeTag"), Descriptor.LifeTag);
	Payload->SetStringField(TEXT("displayName"), Descriptor.DisplayName.ToString());
	Payload->SetNumberField(TEXT("health"), Descriptor.Health);
	Payload->SetNumberField(TEXT("maxHealth"), Descriptor.MaxHealth);
	Payload->SetBoolField(TEXT("attackAllowed"), Descriptor.bLocallyAttackAllowed);
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
	if (PayloadJson == LastInteractionPayloadJson) return;
	LastInteractionPayloadJson = PayloadJson;
	WebUIBridge->SendEvent(TEXT("hud_interaction"), PayloadJson);
}
void AAuraHUD::HandleTargetPreviewForWebUI(const FAuraTargetDescriptor& Descriptor) { SendInteractionToWebUI(Descriptor); }
void AAuraHUD::HandleTargetPreviewClearedForWebUI()
{
	LastInteractionPayloadJson.Empty();
	if (WebUIBridge && WebUIBridge->IsServerRunning()) WebUIBridge->SendEvent(TEXT("hud_interaction_cleared"), TEXT("{}"));
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
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	return SlotTag == Tags.InputTag_LMB || SlotTag == Tags.InputTag_RMB || SlotTag == Tags.InputTag_1 || SlotTag == Tags.InputTag_2
		|| SlotTag == Tags.InputTag_3 || SlotTag == Tags.InputTag_4 || SlotTag == Tags.InputTag_Passive_1 || SlotTag == Tags.InputTag_Passive_2;
}
bool AAuraHUD::TryGetWebAbilityInputTag(const FString& InputTagName, FGameplayTag& OutInputTag) const
{
	OutInputTag = FGameplayTag::RequestGameplayTag(FName(*InputTagName), false);
	return IsKnownSpellSlot(OutInputTag);
}

FString AAuraHUD::BuildAbilityIconDataUri(const UTexture2D* Icon)
{
	if (!Icon) return FString();
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
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController()))
	{
		const FAuraTargetDescriptor& Descriptor = AuraPC->GetFocusedTargetDescriptor();
		if (Descriptor.IsValid()) SendInteractionToWebUI(Descriptor);
	}
}
