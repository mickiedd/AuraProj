// Copyright Druid Mechanics

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "UI/WebUI/WebUIWidget.h"

static TWeakObjectPtr<UWebUIWidget> ActiveWebUI;
static FString ActiveWebUIAssetPath;

static void ToggleAuraWebUIAsset(const FString& AssetPath);

static bool TryAutoOpenAuraConfigEditor(float DeltaTime)
{
	if (!GEngine)
	{
		return true;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			continue;
		}

		if (World->GetFirstPlayerController())
		{
			UE_LOG(LogTemp, Display, TEXT("[WebUI] Auto-opening Config Studio after the first local player world became ready."));
			ToggleAuraWebUIAsset(TEXT("WebUI/config-editor.html"));
			return false;
		}
	}

	return true;
}

static void ToggleAuraWebUIAsset(const FString& AssetPath)
{
	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			continue;
		}

		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (ActiveWebUI.IsValid() && ActiveWebUI->IsInViewport())
			{
				ActiveWebUI->RemoveFromParent();
				ActiveWebUI.Reset();
				if (ActiveWebUIAssetPath == AssetPath)
				{
					ActiveWebUIAssetPath.Reset();
					return;
				}
			}

			ActiveWebUI = CreateWidget<UWebUIWidget>(PlayerController, UWebUIWidget::StaticClass());
			if (ActiveWebUI.IsValid())
			{
				ActiveWebUI->HtmlAssetPath = AssetPath;
				ActiveWebUIAssetPath = AssetPath;
				ActiveWebUI->AddToViewport(100);
			}
			return;
		}
	}
}

static void ToggleAuraWebUI()
{
	ToggleAuraWebUIAsset(TEXT("WebUI/index.html"));
}

static void ToggleAuraConfigEditor()
{
	ToggleAuraWebUIAsset(TEXT("WebUI/config-editor.html"));
}

static FAutoConsoleCommand AuraWebUIToggleCommand(
	TEXT("AuraWebUI.Toggle"),
	TEXT("Toggle the opt-in in-game Web UI in the first local player world."),
	FConsoleCommandDelegate::CreateStatic(&ToggleAuraWebUI));

static FAutoConsoleCommand AuraWebUIConfigEditorCommand(
	TEXT("AuraWebUI.ConfigEditor"),
	TEXT("Toggle the embedded Content/Config JSON editor in the first local player world."),
	FConsoleCommandDelegate::CreateStatic(&ToggleAuraConfigEditor));

class FAuraWebUIModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("AuraConfigEditor")))
		{
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&TryAutoOpenAuraConfigEditor),
				0.25f);
		}
	}
};

IMPLEMENT_MODULE(FAuraWebUIModule, AuraWebUI);
