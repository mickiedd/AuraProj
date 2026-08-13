// Copyright Druid Mechanics

#include "UI/WebUI/WebUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
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
	if (UWorld* World = GetWorld())
	{
		if (UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>())
		{
			if (!Bridge->IsServerRunning())
			{
				Bridge->StartServer();
			}
			ResolvedHtml.ReplaceInline(TEXT("__AURA_WEBSOCKET_URL__"), *Bridge->GetWebSocketUrl());
		}
	}

	WebBrowser->LoadString(ResolvedHtml, TEXT("http://auraui.local/index.html"));
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
