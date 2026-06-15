// BehaviacEditorModule.cpp — 编辑器工具栏：Behaviac BT 工具 + 批处理工具

#include "BehaviacEditorModule.h"
#include "BehaviacEditorStyle.h"
#include "BehaviacEditorToolbarCommands.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "HAL/PlatformProcess.h"
#include "LevelEditor.h"
#include "Interfaces/IPluginManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "BehaviorTree/BehaviacBehaviorTree.h"
#include "EditorReimportHandler.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
// Networking / async helpers for starting local http server and probing port
#include "Async/Async.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

#define LOCTEXT_NAMESPACE "FBehaviacEditorModule"

// Path resolved at runtime relative to the plugin's own directory.
static FString GetBTEditorPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("BehaviacPlugin");
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] BehaviacPlugin not found via IPluginManager!"));
		return FString();
	}
	return Plugin->GetBaseDir() / TEXT("Editor/index.html");
}

void FBehaviacEditorModule::StartupModule()
{
	FBehaviacEditorStyle::Initialize();
	FBehaviacEditorStyle::ReloadTextures();

	// ── Behaviac 命令注册 ─────────────────────────────────────────────────────
	FBehaviacEditorToolbarCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FBehaviacEditorToolbarCommands::Get().OpenBTEditor,
		FExecuteAction::CreateRaw(this, &FBehaviacEditorModule::OnOpenBTEditor),
		FCanExecuteAction());
	/*PluginCommands->MapAction(
		FBehaviacEditorToolbarCommands::Get().ReimportSelectedBT,
		FExecuteAction::CreateRaw(this, &FBehaviacEditorModule::OnReimportSelectedBT),
		FCanExecuteAction());*/
	PluginCommands->MapAction(
		FBehaviacEditorToolbarCommands::Get().ToggleDebugServer,
		FExecuteAction::CreateRaw(this, &FBehaviacEditorModule::OnToggleDebugServer),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &FBehaviacEditorModule::IsDebugServerRunning));
	PluginCommands->MapAction(
		FBehaviacEditorToolbarCommands::Get().ToggleServerDebugServer,
		FExecuteAction::CreateRaw(this, &FBehaviacEditorModule::OnToggleServerDebugServer),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &FBehaviacEditorModule::IsServerDebugServerRunning));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBehaviacEditorModule::RegisterMenus));

	// 监听 PIE 结束事件，自动重置调试服务器状态
	FEditorDelegates::EndPIE.AddRaw(this, &FBehaviacEditorModule::OnPIEEnded);
	FWorldDelegates::OnWorldCleanup.AddRaw(this, &FBehaviacEditorModule::OnWorldCleanup);

	// 监听 XML 源文件修改并自动触发 Reimport（进而自动保存 .uasset）
	AutoReimportTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FBehaviacEditorModule::TickAutoReimportMonitor),
		1.0f);
}

void FBehaviacEditorModule::ShutdownModule()
{
	FEditorDelegates::EndPIE.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	if (AutoReimportTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(AutoReimportTickerHandle);
		AutoReimportTickerHandle.Reset();
	}
	SourceXmlTimestampByAssetPath.Reset();
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	StopLocalHttpServer();

	FBehaviacEditorToolbarCommands::Unregister();
	FBehaviacEditorStyle::Shutdown();
}

void FBehaviacEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// UE5 的 Level Editor 工具栏已迁移到 UToolMenus，旧的 "Settings" extender hook
	// 不再存在（会被静默忽略，按钮不显示）。这里直接扩展 PlayToolBar（Platforms 所在那一行）。
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(
		"LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!ToolbarMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] Failed to extend LevelEditor PlayToolBar menu."));
		return;
	}

	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("BehaviacEditor"));

	FToolMenuEntry ComboEntry = FToolMenuEntry::InitComboButton(
		"BehaviacCombo",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FBehaviacEditorModule::BuildBehaviacMenuWidget),
		LOCTEXT("BehaviacComboLabel",   "Behaviac"),
		LOCTEXT("BehaviacComboTooltip", "Behaviac 行为树工具"),
		FSlateIcon(FBehaviacEditorStyle::GetStyleSetName(), "BehaviacEditor.BehaviacCombo"),
		/*bInSimpleComboBox=*/false);

	Section.AddEntry(ComboEntry);
}

void FBehaviacEditorModule::OnOpenBTEditor()
{
	DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/false);

	const FString LauncherPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("Plugins/BehaviorU/BehaviacLauncher/BehaviacLauncher.exe"));

	if (!FPaths::FileExists(LauncherPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] BehaviacLauncher.exe not found: %s"), *LauncherPath);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Launching BehaviacLauncher: %s"), *LauncherPath);
	FPlatformProcess::CreateProc(
		*LauncherPath,
		/*Params=*/nullptr,
		/*bLaunchDetached=*/true,
		/*bLaunchHidden=*/false,
		/*bLaunchReallyHidden=*/false,
		/*OutProcessID=*/nullptr,
		/*PriorityModifier=*/0,
		/*OptionalWorkingDirectory=*/nullptr,
		/*PipeWriteChild=*/nullptr);
}

void FBehaviacEditorModule::OnReimportSelectedBT()
{
	// Gather selected assets from the Content Browser
	FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	CBModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] ⚠️ No assets selected in the Content Browser. Please select a Behaviac Behavior Tree asset first."));
		return;
	}

	int32 ReimportedCount = 0;
	int32 FailedCount = 0;

	for (const FAssetData& AssetData : SelectedAssets)
	{
		// Load the asset and check if it's a UBehaviacBehaviorTree
		UObject* Asset = AssetData.GetAsset();
		UBehaviacBehaviorTree* BehaviorTree = Cast<UBehaviacBehaviorTree>(Asset);
		if (!BehaviorTree)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] ⚠️ Skipping '%s': not a Behaviac Behavior Tree asset."), *AssetData.AssetName.ToString());
			continue;
		}

		// Verify that there is a source file path recorded
		if (BehaviorTree->SourceFilePath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] ❌ '%s' has no source file path stored. Was it originally imported from XML?"), *BehaviorTree->TreeName);
			++FailedCount;
			continue;
		}

		// Verify the source XML file actually exists on disk
		if (!FPaths::FileExists(BehaviorTree->SourceFilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] ❌ Source XML file not found for '%s': %s"), *BehaviorTree->TreeName, *BehaviorTree->SourceFilePath);
			++FailedCount;
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] 🔄 Reimporting '%s' from: %s"), *BehaviorTree->TreeName, *BehaviorTree->SourceFilePath);

		// Use UE's built-in reimport manager — it will find the FReimportHandler (UBehaviacBehaviorTreeImportFactory)
		const bool bSuccess = FReimportManager::Instance()->Reimport(BehaviorTree, /*bAskForNewFileIfMissing=*/false, /*bShowNotification=*/true);
		if (bSuccess)
		{
			++ReimportedCount;
			UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] ✅ Successfully reimported '%s'"), *BehaviorTree->TreeName);
		}
		else
		{
			++FailedCount;
			UE_LOG(LogTemp, Error, TEXT("[BehaviacEditor] ❌ Failed to reimport '%s'"), *BehaviorTree->TreeName);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] Reimport complete — %d succeeded, %d failed."), ReimportedCount, FailedCount);
}

void FBehaviacEditorModule::OnToggleDebugServer()
{
	if (bDebugServerRunning)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] 停止行为树调试服务器。"));
		if (GEngine)
		{
			GEngine->Exec(nullptr, TEXT("Behaviac.Debug.StopServer"));
		}
		bDebugServerRunning = false;
		return;
	}

	// Enabling this mode must disable all other debugger trigger modes first.
	DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/true, /*bKeepServerDebugMode=*/false);

	bDebugServerRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] 启动行为树调试服务器..."));
	if (GEngine)
	{
		GEngine->Exec(nullptr, TEXT("Behaviac.Debug.StartServer"));
	}
}

bool FBehaviacEditorModule::IsDebugServerRunning() const
{
	return bDebugServerRunning;
}

UWorld* FBehaviacEditorModule::GetClientWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	UWorld* FallbackPIE  = nullptr;
	UWorld* FallbackGame = nullptr;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World)
		{
			continue;
		}

		// 最优先：有 ServerConnection 的 World
		// 覆盖两种场景：
		//   1. PIE 多端模式中的客户端 World
		//   2. 运行时独立客户端连接外部 DS 时的 Game World
		UNetDriver* NetDriver = World->GetNetDriver();
		if (NetDriver && NetDriver->ServerConnection)
		{
			return World;
		}

		// 备选记录（无 ServerConnection 时兜底）
		if (Context.WorldType == EWorldType::PIE)
		{
			FallbackPIE = World;
		}
		else if (Context.WorldType == EWorldType::Game)
		{
			FallbackGame = World;
		}
	}

	// 回退顺序：PIE > Game > nullptr
	if (FallbackPIE)  return FallbackPIE;
	if (FallbackGame) return FallbackGame;
	return nullptr;
}

void FBehaviacEditorModule::DisableOtherDebuggerModes(bool bKeepClientDebugMode, bool bKeepServerDebugMode)
{
	const bool bNeedStopClient = !bKeepClientDebugMode && bDebugServerRunning;
	const bool bNeedStopServer = !bKeepServerDebugMode && bServerDebugServerRunning;

	if (bNeedStopClient && GEngine)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] 关闭 BT Debug（切换到其他调试入口）"));
		GEngine->Exec(nullptr, TEXT("Behaviac.Debug.StopServer"));
	}
	if (bNeedStopClient)
	{
		bDebugServerRunning = false;
	}

	if (bNeedStopServer)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] 关闭 BT DS Debug（切换到其他调试入口）"));
		if (UWorld* ClientWorld = GetClientWorld())
		{
			if (APlayerController* PC = ClientWorld->GetFirstPlayerController())
			{
				PC->ServerExec("Behaviac.Debug.StopServer");
			}
		}
		bServerDebugServerRunning = false;
	}

	if (bNeedStopClient || bNeedStopServer)
	{
		StopLocalHttpServer();
	}
}

void FBehaviacEditorModule::OnToggleServerDebugServer()
{
	if (bServerDebugServerRunning)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] [ServerExec] 停止服务端行为树调试服务器。"));
		if (UWorld* ClientWorld = GetClientWorld())
		{
			if (APlayerController* PC = ClientWorld->GetFirstPlayerController())
			{
				PC->ServerExec("Behaviac.Debug.StopServer");
			}
		}
		bServerDebugServerRunning = false;
		return;
	}

	// Enabling this mode must disable all other debugger trigger modes first.
	DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/true);

	UWorld* ClientWorld = GetClientWorld();
	if (!ClientWorld || !ClientWorld->GetFirstPlayerController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] 服务端调试需要客户端已连接到服务器（PIE 或运行时连接外部 DS）。"));
		bServerDebugServerRunning = false;
		return;
	}

	bServerDebugServerRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] [ServerExec] 启动服务端行为树调试服务器..."));
	ClientWorld->GetFirstPlayerController()->ServerExec("Behaviac.Debug.StartServer");
}

// Keep the local server state static inside this translation unit
static FProcHandle GBehaviacPythonServerProc;
static uint32 GBehaviacPythonServerPID = 0;

bool FBehaviacEditorModule::StartLocalHttpServer(const FString& WorkingDirectory, int32 Port)
{
	// If we already have a running proc, assume server is up
	if (GBehaviacPythonServerProc.IsValid() && FPlatformProcess::IsProcRunning(GBehaviacPythonServerProc))
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Local HTTP server already running (PID=%u)"), GBehaviacPythonServerPID);
		return true;
	}

	// Run our custom server script (behaviac_server.py) which serves the Editor folder and
	// provides a POST /save endpoint to persist XML files.
	const FString ScriptPath = FPaths::Combine(*WorkingDirectory, TEXT("behaviac_server.py"));
	if (!FPaths::FileExists(ScriptPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] behaviac_server.py not found in %s — falling back to python -m http.server"), *WorkingDirectory);
		const FString CmdLine = FString::Printf(TEXT("-m http.server %d --bind 0.0.0.0"), Port);
		uint32 OutPID = 0;
		FString PythonExe = TEXT("python");
		GBehaviacPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *CmdLine, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
		if (!GBehaviacPythonServerProc.IsValid())
		{
			PythonExe = TEXT("py");
			GBehaviacPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *CmdLine, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
		}
		if (GBehaviacPythonServerProc.IsValid())
		{
			GBehaviacPythonServerPID = OutPID;
			UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Started python http.server (PID=%u) in %s"), GBehaviacPythonServerPID, *WorkingDirectory);
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] Failed to start python http.server; ensure Python is in PATH or start server manually."));
		return false;
	}

	uint32 OutPID = 0;
	FString PythonExe = TEXT("python");
	// Use unbuffered mode (-u) to get immediate prints in logs; quote script path
	const FString PythonArgs = FString::Printf(TEXT("-u \"%s\" %d \"%s\""), *ScriptPath, Port, *WorkingDirectory);

	GBehaviacPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *PythonArgs, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
	if (!GBehaviacPythonServerProc.IsValid())
	{
		PythonExe = TEXT("py");
		GBehaviacPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *PythonArgs, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
	}

	if (GBehaviacPythonServerProc.IsValid())
	{
		GBehaviacPythonServerPID = OutPID;
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Started behaviac_server.py (PID=%u) in %s"), GBehaviacPythonServerPID, *WorkingDirectory);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] Failed to start behaviac_server.py; ensure Python is in PATH or start server manually."));
	return false;
}

void FBehaviacEditorModule::StopLocalHttpServer()
{
	if (GBehaviacPythonServerProc.IsValid())
	{
		// Attempt graceful termination
		// FPlatformProcess::TerminateProc(GBehaviacPythonServerProc, true);
		// FPlatformProcess::CloseProc(GBehaviacPythonServerProc);
		GBehaviacPythonServerProc = FProcHandle();
		GBehaviacPythonServerPID = 0;
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Stopped local python http.server"));
	}
}

void FBehaviacEditorModule::WaitForPortAndOpenBrowser(const FString& Url, int32 Port, float TimeoutSecs)
{
	// Capture by value so the lambda owns its data safely on another thread
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Url, Port, TimeoutSecs]()
	{
		ISocketSubsystem* SocketSS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const double Deadline = FPlatformTime::Seconds() + (double)TimeoutSecs;
		bool bReady = false;

		while (FPlatformTime::Seconds() < Deadline)
		{
			// Create a fresh TCP socket each probe attempt
			FSocket* Sock = SocketSS->CreateSocket(NAME_Stream, TEXT("BehaviacProbe"), false);
			if (Sock)
			{
				FIPv4Address ClientAddress;
				if (!FIPv4Address::Parse(TEXT("127.0.0.1"), ClientAddress))
				{
				}
				TSharedRef<FInternetAddr> Addr = SocketSS->CreateInternetAddr();
				bool bIsValid = false;
				Addr->SetIp(ClientAddress.Value);
				Addr->SetPort(Port);

				Sock->SetNonBlocking(true);
				const bool bConnected = Sock->Connect(*Addr);
				// On non-blocking connect, success or EINPROGRESS both mean the port is open
				if (bConnected || SocketSS->GetLastErrorCode() == SE_EINPROGRESS
								|| SocketSS->GetLastErrorCode() == SE_EWOULDBLOCK)
				{
					bReady = true;
					SocketSS->DestroySocket(Sock);
					break;
				}
				SocketSS->DestroySocket(Sock);
			}
			// Wait 200ms before retrying
			FPlatformProcess::Sleep(0.2f);
		}

		if (!bReady)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] Timed out waiting for port %d — opening browser anyway"), Port);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Port %d is ready, opening browser"), Port);
		}

		// Always open the browser (even on timeout) — dispatch back to game thread
		AsyncTask(ENamedThreads::GameThread, [Url]()
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		});
	});
}

bool FBehaviacEditorModule::IsServerDebugServerRunning() const
{
	return bServerDebugServerRunning;
}

void FBehaviacEditorModule::FillBehaviacDropdownMenu(UToolMenu* Menu)
{
	// 编辑器工具分组
	FToolMenuSection& EditorSection = Menu->FindOrAddSection(
		"BehaviacEditorTools",
		LOCTEXT("BehaviacEditorSectionLabel", "编辑器"));
	EditorSection.AddMenuEntryWithCommandList(
		FBehaviacEditorToolbarCommands::Get().OpenBTEditor, PluginCommands);
	//EditorSection.AddMenuEntryWithCommandList(
	//	FBehaviacEditorToolbarCommands::Get().ReimportSelectedBT, PluginCommands);

	// 调试服务器分组
	FToolMenuSection& DebugSection = Menu->FindOrAddSection(
		"BehaviacDebugTools",
		LOCTEXT("BehaviacDebugSectionLabel", "调试"));
	DebugSection.AddMenuEntryWithCommandList(
		FBehaviacEditorToolbarCommands::Get().ToggleDebugServer, PluginCommands);
	DebugSection.AddMenuEntryWithCommandList(
		FBehaviacEditorToolbarCommands::Get().ToggleServerDebugServer, PluginCommands);
}

TSharedRef<SWidget> FBehaviacEditorModule::BuildBehaviacMenuWidget()
{
	// Legacy Slate 路径：用 FMenuBuilder 构造与上方相同的两分组菜单
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, PluginCommands);

	MenuBuilder.BeginSection("BehaviacEditorTools", LOCTEXT("BehaviacEditorSectionLabel", "编辑器"));
	MenuBuilder.AddMenuEntry(FBehaviacEditorToolbarCommands::Get().OpenBTEditor);
	//MenuBuilder.AddMenuEntry(FBehaviacEditorToolbarCommands::Get().ReimportSelectedBT);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("BehaviacDebugTools", LOCTEXT("BehaviacDebugSectionLabel", "调试"));
	MenuBuilder.AddMenuEntry(FBehaviacEditorToolbarCommands::Get().ToggleDebugServer);
	MenuBuilder.AddMenuEntry(FBehaviacEditorToolbarCommands::Get().ToggleServerDebugServer);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FBehaviacEditorModule::OnPIEEnded(bool bIsSimulating)
{
	// PIE 结束时自动重置调试服务器状态
	if (bDebugServerRunning || bServerDebugServerRunning)
	{
		DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/false);
		bDebugServerRunning = false;
		bServerDebugServerRunning = false;
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] PIE 已结束，调试服务器状态已重置"));
	}
}

void FBehaviacEditorModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!World)
	{
		return;
	}

	if (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)
	{
		return;
	}

	if (bDebugServerRunning || bServerDebugServerRunning)
	{
		DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/false);
		bDebugServerRunning = false;
		bServerDebugServerRunning = false;
		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] World cleanup detected (type=%d, sessionEnded=%d, cleanupResources=%d), debug servers stopped."),
			static_cast<int32>(World->WorldType),
			bSessionEnded ? 1 : 0,
			bCleanupResources ? 1 : 0);
	}
}

bool FBehaviacEditorModule::TickAutoReimportMonitor(float DeltaTime)
{
	ScanAndReimportChangedBehaviorTrees();
	return true;
}

void FBehaviacEditorModule::ScanAndReimportChangedBehaviorTrees()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.ClassPaths.Add(UBehaviacBehaviorTree::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> BehaviorTreeAssets;
	AssetRegistryModule.Get().GetAssets(Filter, BehaviorTreeAssets);

	for (const FAssetData& AssetData : BehaviorTreeAssets)
	{
		UBehaviacBehaviorTree* BehaviorTree = Cast<UBehaviacBehaviorTree>(AssetData.GetAsset());
		if (!BehaviorTree || BehaviorTree->SourceFilePath.IsEmpty())
		{
			continue;
		}

		FString NormalizedSourcePath = FPaths::ConvertRelativePathToFull(BehaviorTree->SourceFilePath);
		FPaths::NormalizeFilename(NormalizedSourcePath);

		if (!IFileManager::Get().FileExists(*NormalizedSourcePath))
		{
			continue;
		}

		const FDateTime CurrentTimestamp = IFileManager::Get().GetTimeStamp(*NormalizedSourcePath);
		const FString AssetPathKey = AssetData.ObjectPath.ToString();
		FDateTime* CachedTimestamp = SourceXmlTimestampByAssetPath.Find(AssetPathKey);

		if (!CachedTimestamp)
		{
			SourceXmlTimestampByAssetPath.Add(AssetPathKey, CurrentTimestamp);
			continue;
		}

		if (CurrentTimestamp <= *CachedTimestamp)
		{
			continue;
		}

		*CachedTimestamp = CurrentTimestamp;

		UE_LOG(LogTemp, Log, TEXT("[BehaviacEditor] Detected XML change, auto-reimport '%s' from: %s"),
			*BehaviorTree->TreeName,
			*NormalizedSourcePath);

		const bool bSuccess = FReimportManager::Instance()->Reimport(
			BehaviorTree,
			/*bAskForNewFileIfMissing=*/false,
			/*bShowNotification=*/false);

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BehaviacEditor] Auto-reimport failed for '%s'"), *BehaviorTree->TreeName);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBehaviacEditorModule, BehaviacEditor)
