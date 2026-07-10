// BehaviorUEditorModule.cpp — 编辑器工具栏：BehaviorU BT 工具 + 批处理工具

#include "BehaviorUEditorModule.h"
#include "BehaviorUEditorStyle.h"
#include "BehaviorUEditorToolbarCommands.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "HAL/PlatformProcess.h"
#include "LevelEditor.h"
#include "Interfaces/IPluginManager.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "BehaviorTree/BehaviorUBehaviorTree.h"
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

#define LOCTEXT_NAMESPACE "FBehaviorUEditorModule"

// Path resolved at runtime relative to the plugin's own directory.
static FString GetBTEditorPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("BehaviorUPlugin");
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] BehaviorUPlugin not found via IPluginManager!"));
		return FString();
	}
	return Plugin->GetBaseDir() / TEXT("Editor/index.html");
}

void FBehaviorUEditorModule::StartupModule()
{
	FBehaviorUEditorStyle::Initialize();
	FBehaviorUEditorStyle::ReloadTextures();

	// ── BehaviorU 命令注册 ─────────────────────────────────────────────────────
	FBehaviorUEditorToolbarCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FBehaviorUEditorToolbarCommands::Get().OpenBTEditor,
		FExecuteAction::CreateRaw(this, &FBehaviorUEditorModule::OnOpenBTEditor),
		FCanExecuteAction());
	/*PluginCommands->MapAction(
		FBehaviorUEditorToolbarCommands::Get().ReimportSelectedBT,
		FExecuteAction::CreateRaw(this, &FBehaviorUEditorModule::OnReimportSelectedBT),
		FCanExecuteAction());*/
	PluginCommands->MapAction(
		FBehaviorUEditorToolbarCommands::Get().ToggleDebugServer,
		FExecuteAction::CreateRaw(this, &FBehaviorUEditorModule::OnToggleDebugServer),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &FBehaviorUEditorModule::IsDebugServerRunning));
	PluginCommands->MapAction(
		FBehaviorUEditorToolbarCommands::Get().ToggleServerDebugServer,
		FExecuteAction::CreateRaw(this, &FBehaviorUEditorModule::OnToggleServerDebugServer),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &FBehaviorUEditorModule::IsServerDebugServerRunning));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBehaviorUEditorModule::RegisterMenus));

	// 监听 PIE 结束事件，自动重置调试服务器状态
	FEditorDelegates::EndPIE.AddRaw(this, &FBehaviorUEditorModule::OnPIEEnded);
	FWorldDelegates::OnWorldCleanup.AddRaw(this, &FBehaviorUEditorModule::OnWorldCleanup);

	// 监听 XML 源文件修改并自动触发 Reimport（进而自动保存 .uasset）
	AutoReimportTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FBehaviorUEditorModule::TickAutoReimportMonitor),
		1.0f);
}

void FBehaviorUEditorModule::ShutdownModule()
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

	FBehaviorUEditorToolbarCommands::Unregister();
	FBehaviorUEditorStyle::Shutdown();
}

void FBehaviorUEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// UE5 的 Level Editor 工具栏已迁移到 UToolMenus，旧的 "Settings" extender hook
	// 不再存在（会被静默忽略，按钮不显示）。这里直接扩展 PlayToolBar（Platforms 所在那一行）。
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(
		"LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!ToolbarMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] Failed to extend LevelEditor PlayToolBar menu."));
		return;
	}

	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("BehaviorUEditor"));

	FToolMenuEntry ComboEntry = FToolMenuEntry::InitComboButton(
		"BehaviorUCombo",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FBehaviorUEditorModule::BuildBehaviorUMenuWidget),
		LOCTEXT("BehaviorUComboLabel",   "BehaviorU"),
		LOCTEXT("BehaviorUComboTooltip", "BehaviorU 行为树工具"),
		FSlateIcon(FBehaviorUEditorStyle::GetStyleSetName(), "BehaviorUEditor.BehaviorUCombo"),
		/*bInSimpleComboBox=*/false);

	Section.AddEntry(ComboEntry);
}

void FBehaviorUEditorModule::OnOpenBTEditor()
{
	DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/false);

	const FString LauncherPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("Plugins/BehaviorU/BehaviorULauncher/BehaviorULauncher.exe"));

	if (!FPaths::FileExists(LauncherPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] BehaviorULauncher.exe not found: %s"), *LauncherPath);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Launching BehaviorULauncher: %s"), *LauncherPath);
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

void FBehaviorUEditorModule::OnReimportSelectedBT()
{
	// Gather selected assets from the Content Browser
	FContentBrowserModule& CBModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	CBModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] ⚠️ No assets selected in the Content Browser. Please select a BehaviorU Behavior Tree asset first."));
		return;
	}

	int32 ReimportedCount = 0;
	int32 FailedCount = 0;

	for (const FAssetData& AssetData : SelectedAssets)
	{
		// Load the asset and check if it's a UBehaviorUBehaviorTree
		UObject* Asset = AssetData.GetAsset();
		UBehaviorUBehaviorTree* BehaviorTree = Cast<UBehaviorUBehaviorTree>(Asset);
		if (!BehaviorTree)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] ⚠️ Skipping '%s': not a BehaviorU Behavior Tree asset."), *AssetData.AssetName.ToString());
			continue;
		}

		// Verify that there is a source file path recorded
		if (BehaviorTree->SourceFilePath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] ❌ '%s' has no source file path stored. Was it originally imported from XML?"), *BehaviorTree->TreeName);
			++FailedCount;
			continue;
		}

		// Verify the source XML file actually exists on disk
		if (!FPaths::FileExists(BehaviorTree->SourceFilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] ❌ Source XML file not found for '%s': %s"), *BehaviorTree->TreeName, *BehaviorTree->SourceFilePath);
			++FailedCount;
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] 🔄 Reimporting '%s' from: %s"), *BehaviorTree->TreeName, *BehaviorTree->SourceFilePath);

		// Use UE's built-in reimport manager — it will find the FReimportHandler (UBehaviorUBehaviorTreeImportFactory)
		const bool bSuccess = FReimportManager::Instance()->Reimport(BehaviorTree, /*bAskForNewFileIfMissing=*/false, /*bShowNotification=*/true);
		if (bSuccess)
		{
			++ReimportedCount;
			UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] ✅ Successfully reimported '%s'"), *BehaviorTree->TreeName);
		}
		else
		{
			++FailedCount;
			UE_LOG(LogTemp, Error, TEXT("[BehaviorUEditor] ❌ Failed to reimport '%s'"), *BehaviorTree->TreeName);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] Reimport complete — %d succeeded, %d failed."), ReimportedCount, FailedCount);
}

void FBehaviorUEditorModule::OnToggleDebugServer()
{
	if (bDebugServerRunning)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] 停止行为树调试服务器。"));
		if (GEngine)
		{
			GEngine->Exec(nullptr, TEXT("BehaviorU.Debug.StopServer"));
		}
		bDebugServerRunning = false;
		return;
	}

	// Enabling this mode must disable all other debugger trigger modes first.
	DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/true, /*bKeepServerDebugMode=*/false);

	bDebugServerRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] 启动行为树调试服务器..."));
	if (GEngine)
	{
		GEngine->Exec(nullptr, TEXT("BehaviorU.Debug.StartServer"));
	}
}

bool FBehaviorUEditorModule::IsDebugServerRunning() const
{
	return bDebugServerRunning;
}

UWorld* FBehaviorUEditorModule::GetClientWorld()
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

void FBehaviorUEditorModule::DisableOtherDebuggerModes(bool bKeepClientDebugMode, bool bKeepServerDebugMode)
{
	const bool bNeedStopClient = !bKeepClientDebugMode && bDebugServerRunning;
	const bool bNeedStopServer = !bKeepServerDebugMode && bServerDebugServerRunning;

	if (bNeedStopClient && GEngine)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] 关闭 BT Debug（切换到其他调试入口）"));
		GEngine->Exec(nullptr, TEXT("BehaviorU.Debug.StopServer"));
	}
	if (bNeedStopClient)
	{
		bDebugServerRunning = false;
	}

	if (bNeedStopServer)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] 关闭 BT DS Debug（切换到其他调试入口）"));
		if (UWorld* ClientWorld = GetClientWorld())
		{
			if (APlayerController* PC = ClientWorld->GetFirstPlayerController())
			{
				PC->ServerExec("BehaviorU.Debug.StopServer");
			}
		}
		bServerDebugServerRunning = false;
	}

	if (bNeedStopClient || bNeedStopServer)
	{
		StopLocalHttpServer();
	}
}

void FBehaviorUEditorModule::OnToggleServerDebugServer()
{
	if (bServerDebugServerRunning)
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] [ServerExec] 停止服务端行为树调试服务器。"));
		if (UWorld* ClientWorld = GetClientWorld())
		{
			if (APlayerController* PC = ClientWorld->GetFirstPlayerController())
			{
				PC->ServerExec("BehaviorU.Debug.StopServer");
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
		UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] 服务端调试需要客户端已连接到服务器（PIE 或运行时连接外部 DS）。"));
		bServerDebugServerRunning = false;
		return;
	}

	bServerDebugServerRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] [ServerExec] 启动服务端行为树调试服务器..."));
	ClientWorld->GetFirstPlayerController()->ServerExec("BehaviorU.Debug.StartServer");
}

// Keep the local server state static inside this translation unit
static FProcHandle GBehaviorUPythonServerProc;
static uint32 GBehaviorUPythonServerPID = 0;

bool FBehaviorUEditorModule::StartLocalHttpServer(const FString& WorkingDirectory, int32 Port)
{
	// If we already have a running proc, assume server is up
	if (GBehaviorUPythonServerProc.IsValid() && FPlatformProcess::IsProcRunning(GBehaviorUPythonServerProc))
	{
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Local HTTP server already running (PID=%u)"), GBehaviorUPythonServerPID);
		return true;
	}

	// Run our custom server script (behavioru_server.py) which serves the Editor folder and
	// provides a POST /save endpoint to persist XML files.
	const FString ScriptPath = FPaths::Combine(*WorkingDirectory, TEXT("behavioru_server.py"));
	if (!FPaths::FileExists(ScriptPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] behavioru_server.py not found in %s — falling back to python -m http.server"), *WorkingDirectory);
		const FString CmdLine = FString::Printf(TEXT("-m http.server %d --bind 0.0.0.0"), Port);
		uint32 OutPID = 0;
		FString PythonExe = TEXT("python");
		GBehaviorUPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *CmdLine, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
		if (!GBehaviorUPythonServerProc.IsValid())
		{
			PythonExe = TEXT("py");
			GBehaviorUPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *CmdLine, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
		}
		if (GBehaviorUPythonServerProc.IsValid())
		{
			GBehaviorUPythonServerPID = OutPID;
			UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Started python http.server (PID=%u) in %s"), GBehaviorUPythonServerPID, *WorkingDirectory);
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] Failed to start python http.server; ensure Python is in PATH or start server manually."));
		return false;
	}

	uint32 OutPID = 0;
	FString PythonExe = TEXT("python");
	// Use unbuffered mode (-u) to get immediate prints in logs; quote script path
	const FString PythonArgs = FString::Printf(TEXT("-u \"%s\" %d \"%s\""), *ScriptPath, Port, *WorkingDirectory);

	GBehaviorUPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *PythonArgs, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
	if (!GBehaviorUPythonServerProc.IsValid())
	{
		PythonExe = TEXT("py");
		GBehaviorUPythonServerProc = FPlatformProcess::CreateProc(*PythonExe, *PythonArgs, false, false, false, &OutPID, 0, *WorkingDirectory, nullptr);
	}

	if (GBehaviorUPythonServerProc.IsValid())
	{
		GBehaviorUPythonServerPID = OutPID;
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Started behavioru_server.py (PID=%u) in %s"), GBehaviorUPythonServerPID, *WorkingDirectory);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] Failed to start behavioru_server.py; ensure Python is in PATH or start server manually."));
	return false;
}

void FBehaviorUEditorModule::StopLocalHttpServer()
{
	if (GBehaviorUPythonServerProc.IsValid())
	{
		// Attempt graceful termination
		// FPlatformProcess::TerminateProc(GBehaviorUPythonServerProc, true);
		// FPlatformProcess::CloseProc(GBehaviorUPythonServerProc);
		GBehaviorUPythonServerProc = FProcHandle();
		GBehaviorUPythonServerPID = 0;
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Stopped local python http.server"));
	}
}

void FBehaviorUEditorModule::WaitForPortAndOpenBrowser(const FString& Url, int32 Port, float TimeoutSecs)
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
			FSocket* Sock = SocketSS->CreateSocket(NAME_Stream, TEXT("BehaviorUProbe"), false);
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
			UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] Timed out waiting for port %d — opening browser anyway"), Port);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Port %d is ready, opening browser"), Port);
		}

		// Always open the browser (even on timeout) — dispatch back to game thread
		AsyncTask(ENamedThreads::GameThread, [Url]()
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		});
	});
}

bool FBehaviorUEditorModule::IsServerDebugServerRunning() const
{
	return bServerDebugServerRunning;
}

void FBehaviorUEditorModule::FillBehaviorUDropdownMenu(UToolMenu* Menu)
{
	// 编辑器工具分组
	FToolMenuSection& EditorSection = Menu->FindOrAddSection(
		"BehaviorUEditorTools",
		LOCTEXT("BehaviorUEditorSectionLabel", "编辑器"));
	EditorSection.AddMenuEntryWithCommandList(
		FBehaviorUEditorToolbarCommands::Get().OpenBTEditor, PluginCommands);
	//EditorSection.AddMenuEntryWithCommandList(
	//	FBehaviorUEditorToolbarCommands::Get().ReimportSelectedBT, PluginCommands);

	// 调试服务器分组
	FToolMenuSection& DebugSection = Menu->FindOrAddSection(
		"BehaviorUDebugTools",
		LOCTEXT("BehaviorUDebugSectionLabel", "调试"));
	DebugSection.AddMenuEntryWithCommandList(
		FBehaviorUEditorToolbarCommands::Get().ToggleDebugServer, PluginCommands);
	DebugSection.AddMenuEntryWithCommandList(
		FBehaviorUEditorToolbarCommands::Get().ToggleServerDebugServer, PluginCommands);
}

TSharedRef<SWidget> FBehaviorUEditorModule::BuildBehaviorUMenuWidget()
{
	// Legacy Slate 路径：用 FMenuBuilder 构造与上方相同的两分组菜单
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, PluginCommands);

	MenuBuilder.BeginSection("BehaviorUEditorTools", LOCTEXT("BehaviorUEditorSectionLabel", "编辑器"));
	MenuBuilder.AddMenuEntry(FBehaviorUEditorToolbarCommands::Get().OpenBTEditor);
	//MenuBuilder.AddMenuEntry(FBehaviorUEditorToolbarCommands::Get().ReimportSelectedBT);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("BehaviorUDebugTools", LOCTEXT("BehaviorUDebugSectionLabel", "调试"));
	MenuBuilder.AddMenuEntry(FBehaviorUEditorToolbarCommands::Get().ToggleDebugServer);
	MenuBuilder.AddMenuEntry(FBehaviorUEditorToolbarCommands::Get().ToggleServerDebugServer);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FBehaviorUEditorModule::OnPIEEnded(bool bIsSimulating)
{
	// PIE 结束时自动重置调试服务器状态
	if (bDebugServerRunning || bServerDebugServerRunning)
	{
		DisableOtherDebuggerModes(/*bKeepClientDebugMode=*/false, /*bKeepServerDebugMode=*/false);
		bDebugServerRunning = false;
		bServerDebugServerRunning = false;
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] PIE 已结束，调试服务器状态已重置"));
	}
}

void FBehaviorUEditorModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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
		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] World cleanup detected (type=%d, sessionEnded=%d, cleanupResources=%d), debug servers stopped."),
			static_cast<int32>(World->WorldType),
			bSessionEnded ? 1 : 0,
			bCleanupResources ? 1 : 0);
	}
}

bool FBehaviorUEditorModule::TickAutoReimportMonitor(float DeltaTime)
{
	ScanAndReimportChangedBehaviorTrees();
	return true;
}

void FBehaviorUEditorModule::ScanAndReimportChangedBehaviorTrees()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.ClassPaths.Add(UBehaviorUBehaviorTree::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> BehaviorTreeAssets;
	AssetRegistryModule.Get().GetAssets(Filter, BehaviorTreeAssets);

	for (const FAssetData& AssetData : BehaviorTreeAssets)
	{
		UBehaviorUBehaviorTree* BehaviorTree = Cast<UBehaviorUBehaviorTree>(AssetData.GetAsset());
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

		UE_LOG(LogTemp, Log, TEXT("[BehaviorUEditor] Detected XML change, auto-reimport '%s' from: %s"),
			*BehaviorTree->TreeName,
			*NormalizedSourcePath);

		const bool bSuccess = FReimportManager::Instance()->Reimport(
			BehaviorTree,
			/*bAskForNewFileIfMissing=*/false,
			/*bShowNotification=*/false);

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BehaviorUEditor] Auto-reimport failed for '%s'"), *BehaviorTree->TreeName);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBehaviorUEditorModule, BehaviorUEditor)
