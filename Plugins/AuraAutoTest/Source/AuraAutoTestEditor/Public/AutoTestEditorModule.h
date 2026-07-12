// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FExtender;

class FAutoTestEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Register the dockable results panel tab spawner. */
	void RegisterTabSpawner();

	/** Open the results panel tab (called by the toolbar button / AutoTest.OpenPanel). */
	static void OpenResultsPanel();

private:
	void RegisterToolbar();
	void RegisterToolMenuToolbar();
	void AddLegacyToolbarButton(FToolBarBuilder& Builder);

	/** Bind to the runner's OnRequestOpenPanel delegate so the console command
	 *  can open the tab without the runtime module depending on Slate. */
	void BindRunnerDelegates(bool bIsSimulating);
	void UnbindRunnerDelegates();

	// Handle bound to a PIE runner instance; rebind on PIE start.
	FDelegateHandle RequestOpenPanelHandle;

	TSharedPtr<FExtender> ToolbarExtender;
};