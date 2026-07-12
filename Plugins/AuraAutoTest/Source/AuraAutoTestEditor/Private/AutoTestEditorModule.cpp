// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestEditorModule.h"
#include "AutoTestLog.h"
#include "AutoTestRunnerSubsystem.h"
#include "SAutoTestPanel.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FAutoTestEditorModule"

static const FName AutoTestResultsTabName(TEXT("AutoTestResults"));

void FAutoTestEditorModule::StartupModule()
{
	RegisterTabSpawner();
	RegisterToolbar();

	// Re-bind to the runner's OnRequestOpenPanel whenever a PIE session starts so the
	// AutoTest.OpenPanel console command (raised by the runtime subsystem) opens the tab.
	FEditorDelegates::BeginPIE.AddRaw(this, &FAutoTestEditorModule::BindRunnerDelegates);
	BindRunnerDelegates(/*bIsSimulating=*/false);

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Editor module started."));
}

void FAutoTestEditorModule::ShutdownModule()
{
	FEditorDelegates::BeginPIE.RemoveAll(this);
	UnbindRunnerDelegates();

	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		if (ToolbarExtender.IsValid())
		{
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);
			ToolbarExtender.Reset();
		}
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterTabSpawner(AutoTestResultsTabName);

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Editor module shut down."));
}

void FAutoTestEditorModule::RegisterTabSpawner()
{
	FOnSpawnTab SpawnTabDelegate = FOnSpawnTab::CreateLambda(
		[](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SAutoTestPanel)
				];
		});

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AutoTestResultsTabName, SpawnTabDelegate);
}

void FAutoTestEditorModule::RegisterToolbar()
{
	// UToolMenus path (primary).
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAutoTestEditorModule::RegisterToolMenuToolbar));
	}

	// Legacy extender fallback.
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension("Play", EExtensionHook::After, nullptr,
		FToolBarExtensionDelegate::CreateRaw(this, &FAutoTestEditorModule::AddLegacyToolbarButton));
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
}

void FAutoTestEditorModule::RegisterToolMenuToolbar()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	if (UToolMenu* Menu = UToolMenus::Get()->FindMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("Play");

		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			"AutoTestResults",
			FUIAction(FExecuteAction::CreateStatic(&FAutoTestEditorModule::OpenResultsPanel)),
			FText::FromString(TEXT("AutoTest")),
			FText::FromString(TEXT("Open the Aura AutoTest results panel")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Symbols.Check"));
		Section.AddEntry(Entry);
	}
}

void FAutoTestEditorModule::AddLegacyToolbarButton(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateStatic(&FAutoTestEditorModule::OpenResultsPanel)),
		NAME_None,
		FText::FromString(TEXT("AutoTest")),
		FText::FromString(TEXT("Open the Aura AutoTest results panel")),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Symbols.Check"));
}

void FAutoTestEditorModule::OpenResultsPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(AutoTestResultsTabName);
}

void FAutoTestEditorModule::BindRunnerDelegates(bool bIsSimulating)
{
	// Find the PIE runner and bind its OnRequestOpenPanel so the console command opens the tab.
	if (!GEngine)
	{
		return;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->WorldType != EWorldType::PIE)
		{
			continue;
		}
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UAutoTestRunnerSubsystem* Runner = GI->GetSubsystem<UAutoTestRunnerSubsystem>())
			{
				if (!RequestOpenPanelHandle.IsValid())
				{
					RequestOpenPanelHandle = Runner->OnRequestOpenPanel().AddStatic(&FAutoTestEditorModule::OpenResultsPanel);
				}
				return;
			}
		}
	}
}

void FAutoTestEditorModule::UnbindRunnerDelegates()
{
	RequestOpenPanelHandle.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAutoTestEditorModule, AuraAutoTestEditor)