// Copyright Druid Mechanics

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "UI/WebUI/WebUIWidget.h"

IMPLEMENT_MODULE(FDefaultGameModuleImpl, AuraWebUI);

static TWeakObjectPtr<UWebUIWidget> ActiveWebUI;

static void ToggleAuraWebUI()
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
				return;
			}

			ActiveWebUI = CreateWidget<UWebUIWidget>(PlayerController, UWebUIWidget::StaticClass());
		if (ActiveWebUI.IsValid())
			{
				ActiveWebUI->AddToViewport(100);
			}
			return;
		}
	}
}

static FAutoConsoleCommand AuraWebUIToggleCommand(
	TEXT("AuraWebUI.Toggle"),
	TEXT("Toggle the opt-in in-game Web UI in the first local player world."),
	FConsoleCommandDelegate::CreateStatic(&ToggleAuraWebUI));
