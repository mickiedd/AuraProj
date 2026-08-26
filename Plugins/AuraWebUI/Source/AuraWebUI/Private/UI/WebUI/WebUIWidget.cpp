// Copyright Druid Mechanics

#include "UI/WebUI/WebUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/PanelWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UObject/UnrealType.h"
#include "WebBrowser.h"

void UWebUIWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWebBrowser();
}

void UWebUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWebBrowser();
	if (bAutoConnectBridge)
	{
		ReloadWebUI();
	}
}

void UWebUIWidget::NativeDestruct()
{
	WebBrowser = nullptr;
	Super::NativeDestruct();
}

void UWebUIWidget::ConfigureViewportLayout(const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment)
{
	if (UGameViewportSubsystem* Subsystem = UGameViewportSubsystem::Get(GetWorld()))
	{
		FGameViewportWidgetSlot ViewportSlot;
		ViewportSlot.Anchors = Anchors;
		ViewportSlot.Offsets = Offsets;
		ViewportSlot.Alignment = Alignment;
		Subsystem->SetWidgetSlot(this, ViewportSlot);
		return;
	}

	// The subsystem is normally available for game widgets. Keep a sensible
	// fallback for editor previews where it may not have initialized yet.
	SetAnchorsInViewport(Anchors);
	SetAlignmentInViewport(Alignment);
}

void UWebUIWidget::EnsureWebBrowser()
{
	if (WebBrowser || !WidgetTree)
	{
		return;
	}

	WebBrowser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("WebBrowser"));
	if (!WebBrowser)
	{
		return;
	}

	// UWebBrowser keeps its transparency switch protected and does not expose a
	// setter. Set the reflected property before TakeWidget() builds SWebBrowser;
	// this makes CEF paint the transparent page background into Slate instead of
	// the default opaque black surface.
	if (FBoolProperty* TransparencyProperty = FindFProperty<FBoolProperty>(WebBrowser->GetClass(), TEXT("bSupportsTransparency")))
	{
		TransparencyProperty->SetPropertyValue_InContainer(WebBrowser, true);
		bBrowserTransparencyEnabled = TransparencyProperty->GetPropertyValue_InContainer(WebBrowser);
	}
	else
	{
		bBrowserTransparencyEnabled = false;
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] UWebBrowser transparency property is unavailable; browser background may be opaque"));
	}

	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WebBrowser;
	}
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetTree->RootWidget))
	{
		Panel->AddChild(WebBrowser);
	}
}

void UWebUIWidget::ReloadWebUI()
{
	const FString Html = LoadHtmlAsset();
	if (Html.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] HTML asset is empty: %s"), *HtmlAssetPath);
		return;
	}
	LoadWebUIHtml(Html);
}

bool UWebUIWidget::LoadWebUIHtml(const FString& Html)
{
	if (!WebBrowser)
	{
		EnsureWebBrowser();
	}
	if (!WebBrowser)
	{
		UE_LOG(LogTemp, Error, TEXT("[WebUI] Could not create UWebBrowser"));
		return false;
	}

	FString ResolvedHtml = Html;
	FString WebSocketUrl;
	if (UWorld* World = GetWorld())
	{
		if (UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>())
		{
			if (!Bridge->IsServerRunning())
			{
				Bridge->StartServer();
			}
			WebSocketUrl = Bridge->GetWebSocketUrl();
			ResolvedHtml.ReplaceInline(TEXT("__AURA_WEBSOCKET_URL__"), *WebSocketUrl);
		}
	}

	WebBrowser->LoadString(ResolvedHtml, TEXT("http://auraui.local/index.html"));
	LastLoadedHtmlAssetPath = HtmlAssetPath;
	LastLoadedHtmlBytes = ResolvedHtml.Len();
	UE_LOG(LogTemp, Display, TEXT("[WebUI] Loaded HTML asset path=%s bytes=%d websocket=%s"),
		*HtmlAssetPath,
		ResolvedHtml.Len(),
		*WebSocketUrl);
	return true;
}

FString UWebUIWidget::LoadHtmlAsset() const
{
	FString Html;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AuraWebUI"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] AuraWebUI plugin was not found while loading %s"), *HtmlAssetPath);
		return FString();
	}

	const FString AbsolutePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content"), HtmlAssetPath);
	if (!FFileHelper::LoadFileToString(Html, *AbsolutePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUI] Failed to load HTML from %s"), *AbsolutePath);
		return FString();
	}
	return Html;
}
