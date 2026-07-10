// BehaviorUEditorModule.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class UWorld;


class FBehaviorUEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();

	// ── BehaviorU 各功能实现 ───────────────────────────────────────────────────
	void OnOpenBTEditor();
	void OnReimportSelectedBT();
	void OnToggleDebugServer();
	bool IsDebugServerRunning() const;
	void OnToggleServerDebugServer();
	bool IsServerDebugServerRunning() const;
	void OnPIEEnded(bool bIsSimulating);
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	// ── BehaviorU 下拉菜单构建 ─────────────────────────────────────────────────
	// UE5 UToolMenus 路径：直接填充 UToolMenu
	void FillBehaviorUDropdownMenu(UToolMenu* Menu);
	// Legacy Slate 路径：返回菜单 Widget（用于 FOnGetContent）
	TSharedRef<SWidget> BuildBehaviorUMenuWidget();

	// ── 工具函数 ──────────────────────────────────────────────────────────────
	static UWorld* GetClientWorld();
	void DisableOtherDebuggerModes(bool bKeepClientDebugMode, bool bKeepServerDebugMode);
	bool StartLocalHttpServer(const FString& WorkingDirectory, int32 Port);
	void StopLocalHttpServer();
	void WaitForPortAndOpenBrowser(const FString& Url, int32 Port, float TimeoutSecs = 8.0f);
	bool TickAutoReimportMonitor(float DeltaTime);
	void ScanAndReimportChangedBehaviorTrees();

	/** BehaviorU 工具条命令列表 */
	TSharedPtr<class FUICommandList> PluginCommands;

	bool bDebugServerRunning       = false;
	bool bServerDebugServerRunning = false;
	FTSTicker::FDelegateHandle AutoReimportTickerHandle;
	TMap<FString, FDateTime> SourceXmlTimestampByAssetPath;

};
