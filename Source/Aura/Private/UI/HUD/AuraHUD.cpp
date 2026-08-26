// Copyright Druid Mechanics


#include "UI/HUD/AuraHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/Widget/AuraUserWidget.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Player/AuraPlayerController.h"
#include "AuraGameplayTags.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UI/WebUI/WebUIWidget.h"
#include "WebBrowser.h"

UOverlayWidgetController* AAuraHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

void AAuraHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OverlayWidgetController)
	{
		OverlayWidgetController->AbilityInfoDelegate.RemoveDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
		OverlayWidgetController->OnHealthChanged.RemoveDynamic(this, &AAuraHUD::HandleHealthChangedForWebUI);
		OverlayWidgetController->OnMaxHealthChanged.RemoveDynamic(this, &AAuraHUD::HandleMaxHealthChangedForWebUI);
		OverlayWidgetController->OnManaChanged.RemoveDynamic(this, &AAuraHUD::HandleManaChangedForWebUI);
		OverlayWidgetController->OnMaxManaChanged.RemoveDynamic(this, &AAuraHUD::HandleMaxManaChangedForWebUI);
	}

	if (WebUIBridge)
	{
		WebUIBridge->OnCommand.RemoveDynamic(this, &AAuraHUD::HandleWebUICommand);
		WebUIBridge->OnConnectionChanged.RemoveDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);
	}

	if (SkillPanelWebUI && IsValid(SkillPanelWebUI))
	{
		SkillPanelWebUI->RemoveFromParent();
	}
	SkillPanelWebUI = nullptr;
	WebUIBridge = nullptr;
	bWebSkillPanelReady = false;
	AbilityIconDataUriCache.Empty();

	Super::EndPlay(EndPlayReason);
}

UAttributeMenuWidgetController* AAuraHUD::GetAttributeMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (AttributeMenuWidgetController == nullptr)
	{
		AttributeMenuWidgetController = NewObject<UAttributeMenuWidgetController>(this, AttributeMenuWidgetControllerClass);
		AttributeMenuWidgetController->SetWidgetControllerParams(WCParams);
		AttributeMenuWidgetController->BindCallbacksToDependencies();
	}
	return AttributeMenuWidgetController;
}

USpellMenuWidgetController* AAuraHUD::GetSpellMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (SpellMenuWidgetController == nullptr)
	{
		SpellMenuWidgetController = NewObject<USpellMenuWidgetController>(this, SpellMenuWidgetControllerClass);
		SpellMenuWidgetController->SetWidgetControllerParams(WCParams);
		SpellMenuWidgetController->BindCallbacksToDependencies();
	}
	return SpellMenuWidgetController;
}

void AAuraHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_AuraHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_AuraHUD"));
	
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UAuraUserWidget>(Widget);

	// WBP_HealthManaSpells assigns each spell globe's InputTag in PreConstruct.
	// Build the Slate tree before publishing initial ability info so those tags are
	// ready when the globe listeners receive their first payload.
	Widget->TakeWidget();
	
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	OverlayWidget->SetWidgetController(WidgetController);
	Widget->AddToViewport();
	InitializeSkillPanelWebUI(PC, WidgetController);

	// Adding the widget can run PreConstruct again, and WBP_SpellGlobe clears its
	// brush there. Publish only after the final attachment lifecycle has completed.
	WidgetController->BroadcastInitialValues();
}

void AAuraHUD::InitializeSkillPanelWebUI(APlayerController* PC, UOverlayWidgetController* WidgetController)
{
	if (!bEnableSkillPanelWebUI || !PC || !PC->IsLocalController() || !WidgetController || SkillPanelWebUI)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	WebUIBridge = World->GetSubsystem<UWebUIBridgeSubsystem>();
	if (!WebUIBridge)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web skill panel skipped: bridge subsystem unavailable"));
		return;
	}

	// Use the local player context in real gameplay so the browser is attached to
	// the same viewport layer as the working login/loading WebUI. Headless
	// automation can spawn a controller without an attached local player, so keep
	// the world context as a safe fallback for that case.
	const bool bUseLocalPlayerContext = PC->GetLocalPlayer() != nullptr;
	if (bUseLocalPlayerContext)
	{
		SkillPanelWebUI = CreateWidget<UWebUIWidget>(PC, UWebUIWidget::StaticClass());
	}
	else
	{
		SkillPanelWebUI = CreateWidget<UWebUIWidget>(World, UWebUIWidget::StaticClass());
	}
	if (!SkillPanelWebUI)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraHUD] Failed to create the web skill panel host"));
		WebUIBridge = nullptr;
		return;
	}

	SkillPanelWebUI->HtmlAssetPath = TEXT("WebUI/skill-panel.html");
	// NativeConstruct normally reloads an auto-connected page.  The HUD needs
	// to mount first so its viewport slot is valid, then perform exactly one
	// explicit load; disabling the construct-time reload prevents duplicate
	// WebSocket clients and stale page instances during PIE travel.
	SkillPanelWebUI->bAutoConnectBridge = false;
	SkillPanelWebUI->ConfigureViewportLayout(
		// Stretch the browser over a real bottom band.  With both vertical
		// anchors at 1.0, the bottom margin is interpreted as the widget height;
		// the previous negative value collapsed the browser to zero height.
		FAnchors(0.04f, 0.72f, 0.96f, 0.98f),
		FMargin(20.f, 0.f, -20.f, 0.f),
		FVector2D(0.f, 0.f));

	WidgetController->AbilityInfoDelegate.AddDynamic(this, &AAuraHUD::HandleAbilityInfoForWebUI);
	WidgetController->OnHealthChanged.AddDynamic(this, &AAuraHUD::HandleHealthChangedForWebUI);
	WidgetController->OnMaxHealthChanged.AddDynamic(this, &AAuraHUD::HandleMaxHealthChangedForWebUI);
	WidgetController->OnManaChanged.AddDynamic(this, &AAuraHUD::HandleManaChangedForWebUI);
	WidgetController->OnMaxManaChanged.AddDynamic(this, &AAuraHUD::HandleMaxManaChangedForWebUI);
	WebUIBridge->OnCommand.AddDynamic(this, &AAuraHUD::HandleWebUICommand);
	WebUIBridge->OnConnectionChanged.AddDynamic(this, &AAuraHUD::HandleWebUIConnectionChanged);

	// Keep the native HUD alive as a fallback while the browser page connects.
	// Once the page reports ready it replaces the native skill/vitals/action strip
	// while retaining the normal overlay menus behind the same command bridge.
	SkillPanelWebUI->AddToViewport(200);
	// Load only after mounting so the browser receives the final viewport context.
	SkillPanelWebUI->ReloadWebUI();
	SkillPanelWebUI->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Partial Web UI skill panel created and anchored to the bottom viewport (context=%s owningPlayer=%s browser=%s)"),
		bUseLocalPlayerContext ? TEXT("LocalPlayer") : TEXT("WorldFallback"),
		*GetNameSafe(SkillPanelWebUI->GetOwningPlayer()),
		*GetNameSafe(SkillPanelWebUI->GetWebBrowser()));
}

void AAuraHUD::SetNativeSkillGlobeVisibility(bool bVisible)
{
	if (!OverlayWidget)
	{
		return;
	}

	TArray<UWidget*> Pending;
	int32 UpdatedCount = 0;
	const TArray<FName> KnownSpellGlobeProperties = {
		TEXT("SpellGlobe_LMB"), TEXT("SpellGlobe_RMB"), TEXT("SpellGlobe_1"), TEXT("SpellGlobe_2"),
		TEXT("SpellGlobe_3"), TEXT("SpellGlobe_4"), TEXT("SpellGlobe_Passive_1"), TEXT("SpellGlobe_Passive_2")
	};
	if (FObjectPropertyBase* HealthManaProperty = FindFProperty<FObjectPropertyBase>(OverlayWidget->GetClass(), TEXT("WBP_HealthManaSpells")))
	{
		if (UUserWidget* HealthManaWidget = Cast<UUserWidget>(HealthManaProperty->GetObjectPropertyValue_InContainer(OverlayWidget)))
		{
			for (const FName& GlobePropertyName : KnownSpellGlobeProperties)
			{
				if (FObjectPropertyBase* GlobeProperty = FindFProperty<FObjectPropertyBase>(HealthManaWidget->GetClass(), GlobePropertyName))
				{
					if (UWidget* Globe = Cast<UWidget>(GlobeProperty->GetObjectPropertyValue_InContainer(HealthManaWidget)))
					{
						Globe->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
						++UpdatedCount;
					}
				}
			}
		}
	}

	if (UWidget* Root = OverlayWidget->GetRootWidget())
	{
		Pending.Add(Root);
	}

	while (!Pending.IsEmpty())
	{
		UWidget* Widget = Pending.Pop(EAllowShrinking::No);
		if (!Widget)
		{
			continue;
		}

		const bool bIsSpellGlobe = Widget->GetName().Contains(TEXT("SpellGlobe"))
			|| Widget->GetClass()->GetName().Contains(TEXT("SpellGlobe"));
		if (bIsSpellGlobe)
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			++UpdatedCount;
			continue;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UWidget* Child = Panel->GetChildAt(Index))
				{
					Pending.Add(Child);
				}
			}
		}
		else if (UUserWidget* NestedWidget = Cast<UUserWidget>(Widget))
		{
			if (UWidget* NestedRoot = NestedWidget->GetRootWidget())
			{
				Pending.Add(NestedRoot);
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Native spell-globe widgets %s for Web UI skill panel (count=%d)"),
		bVisible ? TEXT("shown") : TEXT("hidden"), UpdatedCount);
}

void AAuraHUD::SetNativeVitalsAndActionVisibility(bool bVisible)
{
	if (!OverlayWidget)
	{
		return;
	}

	TArray<UWidget*> Pending;
	if (UWidget* Root = OverlayWidget->GetRootWidget())
	{
		Pending.Add(Root);
	}

	int32 UpdatedCount = 0;
	while (!Pending.IsEmpty())
	{
		UWidget* Widget = Pending.Pop(EAllowShrinking::No);
		if (!Widget)
		{
			continue;
		}

		const FString WidgetName = Widget->GetName();
		const FString ClassName = Widget->GetClass()->GetName();
		const bool bIsVital = WidgetName.Contains(TEXT("HealthGlobe"))
			|| WidgetName.Contains(TEXT("ManaGlobe"))
			|| ClassName.Contains(TEXT("HealthGlobe"))
			|| ClassName.Contains(TEXT("ManaGlobe"));
		const bool bIsActionButton = WidgetName.Contains(TEXT("AttributeMenuButton"))
			|| WidgetName.Contains(TEXT("SpellMenuButton"))
			|| WidgetName.Contains(TEXT("Button_Quit"))
			|| WidgetName.Contains(TEXT("ButtonQuit"))
			|| WidgetName.Contains(TEXT("CloseButton"))
			|| ClassName.Contains(TEXT("AttributeMenuButton"))
			|| ClassName.Contains(TEXT("SpellMenuButton"));

		if (bIsVital || bIsActionButton)
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			++UpdatedCount;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UWidget* Child = Panel->GetChildAt(Index))
				{
					Pending.Add(Child);
				}
			}
		}
		else if (UUserWidget* NestedWidget = Cast<UUserWidget>(Widget))
		{
			if (UWidget* NestedRoot = NestedWidget->GetRootWidget())
			{
				Pending.Add(NestedRoot);
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Native health/mana bars and action buttons %s for Web UI HUD (count=%d)"),
		bVisible ? TEXT("shown") : TEXT("hidden"), UpdatedCount);
}

bool AAuraHUD::TriggerNativeOverlayButton(const TArray<FName>& CandidateNames, const TCHAR* CommandName)
{
	if (!OverlayWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web HUD command %s ignored: native overlay is unavailable"), CommandName);
		return false;
	}

	for (const FName& CandidateName : CandidateNames)
	{
		UWidget* Candidate = OverlayWidget->GetWidgetFromName(CandidateName);
		UButton* Button = nullptr;
		TArray<UWidget*> Pending;
		if (Candidate)
		{
			Pending.Add(Candidate);
		}
		while (!Pending.IsEmpty() && !Button)
		{
			UWidget* Widget = Pending.Pop(EAllowShrinking::No);
			if (!Widget)
			{
				continue;
			}
			if (UButton* FoundButton = Cast<UButton>(Widget))
			{
				Button = FoundButton;
				break;
			}
			if (UUserWidget* NestedWidget = Cast<UUserWidget>(Widget))
			{
				if (UWidget* NestedRoot = NestedWidget->GetRootWidget())
				{
					Pending.Add(NestedRoot);
				}
			}
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
				{
					if (UWidget* Child = Panel->GetChildAt(Index))
					{
						Pending.Add(Child);
					}
				}
			}
		}
		if (Button)
		{
			Button->OnClicked.Broadcast();
			++WebHudForwardedActionCount;
			UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Web HUD command %s forwarded to native overlay button %s"),
				CommandName,
				*CandidateName.ToString());
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Web HUD command %s could not find its native overlay button"), CommandName);
	return false;
}

void AAuraHUD::HandleWebUICommand(const FString& Command, const FString& PayloadJson)
{
	UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Web UI command received: %s"), *Command);
	if (Command == TEXT("skill_panel_ready"))
	{
		bWebSkillPanelReady = true;
		SetNativeSkillGlobeVisibility(false);
		SetNativeVitalsAndActionVisibility(false);
		SendVitalsToWebUI();
		UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Web UI skill panel reported ready; native skill globes are now replaced"));
		if (OverlayWidgetController)
		{
			// The browser can connect after the native startup replay. Re-send all
			// slotted abilities so page readiness never loses the initial state.
			OverlayWidgetController->BroadcastAbilityInfo();
		}
		return;
	}

	if (Command == TEXT("hud_attributes_clicked"))
	{
		TriggerNativeOverlayButton({
			TEXT("AttributeMenuButton"),
			TEXT("Button_Attributes"),
			TEXT("Button_Attribute"),
			TEXT("AttributesButton")
		}, TEXT("hud_attributes_clicked"));
		return;
	}

	if (Command == TEXT("hud_spells_clicked"))
	{
		TriggerNativeOverlayButton({
			TEXT("SpellMenuButton"),
			TEXT("Button_Spells"),
			TEXT("Button_Spell"),
			TEXT("SpellsButton")
		}, TEXT("hud_spells_clicked"));
		return;
	}

	if (Command == TEXT("hud_close_clicked"))
	{
		TriggerNativeOverlayButton({
			TEXT("Button_Quit"),
			TEXT("ButtonQuit"),
			TEXT("CloseButton"),
			TEXT("Button_Close"),
			TEXT("QuitButton")
		}, TEXT("hud_close_clicked"));
		return;
	}

	if (Command != TEXT("skill_ability_pressed")
		&& Command != TEXT("skill_ability_held")
		&& Command != TEXT("skill_ability_released"))
	{
		return;
	}

	TSharedPtr<FJsonObject> Payload;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Ignoring malformed skill-panel command payload for %s"), *Command);
		return;
	}

	FString InputTagName;
	if (!Payload->TryGetStringField(TEXT("inputTag"), InputTagName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Skill-panel command %s omitted inputTag"), *Command);
		return;
	}

	FGameplayTag InputTag;
	if (!TryGetWebAbilityInputTag(InputTagName, InputTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraHUD] Rejected unsupported skill-panel input tag '%s'"), *InputTagName);
		return;
	}

	AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetOwningPlayerController());
	if (!AuraPC)
	{
		return;
	}

	if (Command == TEXT("skill_ability_pressed"))
	{
		AuraPC->WebAbilityInputTagPressed(InputTag);
	}
	else if (Command == TEXT("skill_ability_held"))
	{
		AuraPC->WebAbilityInputTagHeld(InputTag);
	}
	else
	{
		AuraPC->WebAbilityInputTagReleased(InputTag);
	}
}

void AAuraHUD::HandleWebUIConnectionChanged(bool bConnected)
{
	if (!bConnected)
	{
		bWebSkillPanelReady = false;
		SetNativeSkillGlobeVisibility(true);
		SetNativeVitalsAndActionVisibility(true);
		UE_LOG(LogTemp, Display, TEXT("[AuraHUD] Web UI skill panel disconnected; native skill globes restored"));
	}

	if (bConnected && OverlayWidgetController)
	{
		OverlayWidgetController->BroadcastAbilityInfo();
	}
}

void AAuraHUD::HandleHealthChangedForWebUI(float NewValue)
{
	WebHealth = NewValue;
	SendVitalsToWebUI();
}

void AAuraHUD::HandleMaxHealthChangedForWebUI(float NewValue)
{
	WebMaxHealth = NewValue;
	SendVitalsToWebUI();
}

void AAuraHUD::HandleManaChangedForWebUI(float NewValue)
{
	WebMana = NewValue;
	SendVitalsToWebUI();
}

void AAuraHUD::HandleMaxManaChangedForWebUI(float NewValue)
{
	WebMaxMana = NewValue;
	SendVitalsToWebUI();
}

void AAuraHUD::SendVitalsToWebUI()
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning())
	{
		return;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("health"), WebHealth);
	Payload->SetNumberField(TEXT("maxHealth"), WebMaxHealth);
	Payload->SetNumberField(TEXT("mana"), WebMana);
	Payload->SetNumberField(TEXT("maxMana"), WebMaxMana);

	FString PayloadJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);
	Writer->Close();
	WebUIBridge->SendEvent(TEXT("hud_vitals"), PayloadJson);
}

void AAuraHUD::HandleAbilityInfoForWebUI(const FAuraAbilityInfo& Info)
{
	if (!WebUIBridge || !WebUIBridge->IsServerRunning() || !Info.InputTag.IsValid())
	{
		return;
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("inputTag"), Info.InputTag.ToString());
	Payload->SetStringField(TEXT("abilityTag"), Info.AbilityTag.ToString());
	Payload->SetStringField(TEXT("statusTag"), Info.StatusTag.ToString());
	Payload->SetStringField(TEXT("abilityType"), Info.AbilityType.ToString());
	Payload->SetNumberField(TEXT("levelRequirement"), Info.LevelRequirement);
	const bool bClear = Info.AbilityTag.MatchesTagExact(GameplayTags.Abilities_None);
	Payload->SetBoolField(TEXT("clear"), bClear);
	Payload->SetStringField(TEXT("icon"), bClear ? FString() : BuildAbilityIconDataUri(Info.Icon));
	Payload->SetStringField(TEXT("iconName"), Info.Icon ? Info.Icon->GetPathName() : FString());

	FString PayloadJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);
	Writer->Close();
	WebUIBridge->SendEvent(TEXT("skill_panel_ability"), PayloadJson);
}

bool AAuraHUD::TryGetWebAbilityInputTag(const FString& InputTagName, FGameplayTag& OutInputTag) const
{
	OutInputTag = FGameplayTag::RequestGameplayTag(FName(*InputTagName), false);
	if (!OutInputTag.IsValid())
	{
		return false;
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	return OutInputTag == GameplayTags.InputTag_LMB
		|| OutInputTag == GameplayTags.InputTag_RMB
		|| OutInputTag == GameplayTags.InputTag_1
		|| OutInputTag == GameplayTags.InputTag_2
		|| OutInputTag == GameplayTags.InputTag_3
		|| OutInputTag == GameplayTags.InputTag_4
		|| OutInputTag == GameplayTags.InputTag_Passive_1
		|| OutInputTag == GameplayTags.InputTag_Passive_2;
}

FString AAuraHUD::BuildAbilityIconDataUri(const UTexture2D* Icon)
{
	if (!Icon)
	{
		return FString();
	}

	const FString CacheKey = Icon->GetPathName();
	if (const FString* Cached = AbilityIconDataUriCache.Find(CacheKey))
	{
		return *Cached;
	}

	FString DataUri;
	const FTexturePlatformData* PlatformData = Icon->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		AbilityIconDataUriCache.Add(CacheKey, DataUri);
		return DataUri;
	}

	const FTexture2DMipMap& Mip = PlatformData->Mips[0];
	const int32 Width = Mip.SizeX > 0 ? Mip.SizeX : PlatformData->SizeX;
	const int32 Height = Mip.SizeY > 0 ? Mip.SizeY : PlatformData->SizeY;
	const int64 ExpectedBytes = static_cast<int64>(Width) * static_cast<int64>(Height) * sizeof(FColor);
	if (Width <= 0 || Height <= 0 || ExpectedBytes <= 0 || Mip.BulkData.GetBulkDataSize() < ExpectedBytes)
	{
		AbilityIconDataUriCache.Add(CacheKey, DataUri);
		return DataUri;
	}

	const void* RawPixels = Mip.BulkData.LockReadOnly();
	if (!RawPixels)
	{
		AbilityIconDataUriCache.Add(CacheKey, DataUri);
		return DataUri;
	}

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), RawPixels, ExpectedBytes);
	Mip.BulkData.Unlock();

	TArray<uint8> PngBytes;
	FImageUtils::CompressImageArray(Width, Height, Pixels, PngBytes);
	if (PngBytes.Num() > 0)
	{
		DataUri = FString::Printf(TEXT("data:image/png;base64,%s"), *FBase64::Encode(PngBytes));
	}

	AbilityIconDataUriCache.Add(CacheKey, DataUri);
	return DataUri;
}

void AAuraHUD::ToggleLocationDisplay()
{
	bShowLocation = !bShowLocation;
}

void AAuraHUD::DrawHUD()
{
	Super::DrawHUD();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	if (bShowLocation)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			const FVector Loc = Pawn->GetActorLocation();
			const FString LocText = FString::Printf(TEXT("Location: X=%.1f  Y=%.1f  Z=%.1f"), Loc.X, Loc.Y, Loc.Z);
			const float PosX = Canvas ? Canvas->SizeX * 0.01f : 20.f;
			const float PosY = Canvas ? Canvas->SizeY * 0.5f : 40.f;
			DrawText(LocText, FColor::Yellow, PosX, PosY, GEngine->GetLargeFont(), 1.f);
		}
	}

	// The interaction preview is intentionally native so it remains usable even
	// when a project-specific WBP has no bindings yet. The controller still
	// broadcasts the same descriptor for Blueprint UI consumers.
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(PC))
	{
		const FAuraTargetDescriptor& Descriptor = AuraPC->GetFocusedTargetDescriptor();
		if (Descriptor.IsValid() && Canvas)
		{
			FString Prompt = Descriptor.DisplayName.ToString();
			for (int32 Index = 0; Index < Descriptor.InteractionOptions.Num(); ++Index)
			{
				const FAuraInteractionOption& Option = Descriptor.InteractionOptions[Index];
				Prompt += FString::Printf(TEXT("\n%d. %s%s"), Index + 1, *Option.DisplayText.ToString(), Option.bEnabled ? TEXT("") : TEXT(" (unavailable)"));
			}
			DrawText(Prompt, FColor::White, Canvas->SizeX * 0.70f, Canvas->SizeY * 0.72f, GEngine->GetLargeFont(), 0.8f);
		}
	}
}
