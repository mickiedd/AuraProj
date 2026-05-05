// Copyright Druid Mechanics

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FAuraEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogAuraEditor, Log, All);

class FAuraEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Startup begin | ProjectDir='%s' | ToolMenuUIEnabled=%s | LevelEditorAlreadyLoaded=%s"),
			*FPaths::ProjectDir(),
			UToolMenus::IsToolMenuUIEnabled() ? TEXT("true") : TEXT("false"),
			FModuleManager::Get().IsModuleLoaded("LevelEditor") ? TEXT("true") : TEXT("false"));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		ToolbarExtender = MakeShared<FExtender>();
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender created | IsValid=%s"), ToolbarExtender.IsValid() ? TEXT("true") : TEXT("false"));

		ToolbarExtender->AddToolBarExtension(
			"Play",
			EExtensionHook::After,
			nullptr,
			FToolBarExtensionDelegate::CreateRaw(this, &FAuraEditorModule::AddLegacyToolbarButton));
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender registered on hook='Play' (After)"));

		// Register immediately if ToolMenus is already initialized.
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Immediate ToolMenus registration triggered"));
			RegisterMenus();
		}
		else
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Immediate ToolMenus registration skipped because UI is disabled"));
		}

		// Also register via startup callback to cover first-time menu construction.
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAuraEditorModule::RegisterMenus));
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus startup callback registered"));
		UE_LOG(LogAuraEditor, Display, TEXT("Startup complete"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Shutdown begin"));

		if (ToolbarExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);
			ToolbarExtender.Reset();
			UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender removed"));
		}
		else
		{
			UE_LOG(LogAuraEditor, Display, TEXT("Legacy extender removal skipped | IsValid=%s | LevelEditorLoaded=%s"),
				ToolbarExtender.IsValid() ? TEXT("true") : TEXT("false"),
				FModuleManager::Get().IsModuleLoaded("LevelEditor") ? TEXT("true") : TEXT("false"));
		}

		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus callbacks/owner unregistered"));
		UE_LOG(LogAuraEditor, Display, TEXT("Shutdown complete"));
	}

private:
	void AddLegacyToolbarButton(FToolBarBuilder& ToolbarBuilder)
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy toolbar builder callback fired; adding combo button"));

		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FAuraEditorModule::GenerateLaunchMenuContent),
			LOCTEXT("LaunchMenuLabel", "Launch"),
			LOCTEXT("LaunchMenuTooltip", "Open launch and build actions for Aura."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));
	}

	TSharedRef<SWidget> GenerateLaunchMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(
			GetDedicatedServerMenuLabel(),
			GetDedicatedServerMenuTooltip(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnStartDedicatedServerClicked)));

		MenuBuilder.AddMenuEntry(
			GetBuildClientMenuLabel(),
			GetBuildClientMenuTooltip(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.PackageProject"),
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnBuildClientClicked)));

		return MenuBuilder.MakeWidget();
	}

	FText GetDedicatedServerMenuLabel() const
	{
#if PLATFORM_MAC
		return LOCTEXT("StartDedicatedServerLabel", "Launch Dedicated Server");
#else
		return LOCTEXT("StartDedicatedServerLabel", "Launch StartDedicatedServer");
#endif
	}

	FText GetDedicatedServerMenuTooltip() const
	{
#if PLATFORM_MAC
		return LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.command from the project root in Terminal.");
#else
		return LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.bat from the project root.");
#endif
	}

	FText GetBuildClientMenuLabel() const
	{
#if PLATFORM_MAC
		return LOCTEXT("BuildClientLabel", "Build Mac Client");
#else
		return LOCTEXT("BuildClientLabel", "Build Windows Client");
#endif
	}

	FText GetBuildClientMenuTooltip() const
	{
#if PLATFORM_MAC
		return LOCTEXT("BuildClientTooltip", "Build and package the Mac Shipping game client with cooked content in Terminal.");
#else
		return LOCTEXT("BuildClientTooltip", "Build and package the Windows Shipping game client with cooked content in a visible console window.");
#endif
	}

	bool RegisterOnMenuPath(const TCHAR* MenuPath)
	{
		UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus probe | Path='%s'"), MenuPath);

		if (UToolMenu* ToolBarMenu = UToolMenus::Get()->FindMenu(MenuPath))
		{
			FToolMenuSection& Section = ToolBarMenu->FindOrAddSection("Play");

			FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
				"StartDedicatedServer",
				FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnStartDedicatedServerClicked)),
				LOCTEXT("StartDedicatedServerLabel", "Dedicated Server"),
				GetDedicatedServerMenuTooltip(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));

			Section.AddEntry(Entry);
			UE_LOG(LogAuraEditor, Display, TEXT("ToolMenus button registered | Path='%s' | Section='Play'"), MenuPath);
			return true;
		}

		UE_LOG(LogAuraEditor, Warning, TEXT("ToolMenus menu not found | Path='%s'"), MenuPath);
		return false;
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UE_LOG(LogAuraEditor, Display, TEXT("RegisterMenus invoked | ToolMenuUIEnabled=%s"), UToolMenus::IsToolMenuUIEnabled() ? TEXT("true") : TEXT("false"));

		const bool bRegisteredOnPlayToolBar = RegisterOnMenuPath(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));
		const bool bRegisteredOnRootToolBar = RegisterOnMenuPath(TEXT("LevelEditor.LevelEditorToolBar"));
		UE_LOG(LogAuraEditor, Display, TEXT("RegisterMenus result | PlayToolBar=%s | RootToolBar=%s"),
			bRegisteredOnPlayToolBar ? TEXT("true") : TEXT("false"),
			bRegisteredOnRootToolBar ? TEXT("true") : TEXT("false"));

		if (!bRegisteredOnPlayToolBar && !bRegisteredOnRootToolBar)
		{
			UE_LOG(LogAuraEditor, Error, TEXT("RegisterMenus failed: no known ToolMenus path accepted the button."));
		}
	}

	bool LaunchProjectScript(const FString& RelativeScriptPath, const FText& MissingScriptDialogText, const FString& FailureDialogText, const FString& SuccessLogLabel) const
	{
		const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeScriptPath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved project script path | Label='%s' | Path='%s'"), *SuccessLogLabel, *ScriptPath);

		if (!FPaths::FileExists(ScriptPath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Launch aborted: script file does not exist | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, MissingScriptDialogText);
			return false;
		}

		return LaunchInVisibleConsole(
			FString::Printf(TEXT("\"%s\""), *ScriptPath),
			FailureDialogText,
			SuccessLogLabel);
	}

	bool LaunchInVisibleConsole(const FString& CommandToRun, const FString& FailureDialogText, const FString& SuccessLogLabel) const
	{
#if PLATFORM_WINDOWS
		const FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
		const FString Executable = CmdExe.IsEmpty() ? TEXT("cmd.exe") : CmdExe;
		const FString Params = FString::Printf(TEXT("/k \"%s\""), *CommandToRun);
		const bool bLaunchDetached = false;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessId = 0;

		UE_LOG(LogAuraEditor, Display, TEXT("Launching process | Label='%s' | Executable='%s' | Params='%s' | WorkingDir='%s' | Detached=%s | Hidden=%s | ReallyHidden=%s"),
			*SuccessLogLabel,
			*Executable,
			*Params,
			*FPaths::ProjectDir(),
			bLaunchDetached ? TEXT("true") : TEXT("false"),
			bLaunchHidden ? TEXT("true") : TEXT("false"),
			bLaunchReallyHidden ? TEXT("true") : TEXT("false"));

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*Executable,
			*Params,
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			&ProcessId,
			0,
			*FPaths::ProjectDir(),
			nullptr);

		if (!ProcHandle.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("CreateProc failed | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FailureDialogText));
			return false;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("CreateProc succeeded | Label='%s' | PID=%u"), *SuccessLogLabel, ProcessId);
		FPlatformProcess::CloseProc(ProcHandle);
		UE_LOG(LogAuraEditor, Display, TEXT("Process handle closed after successful launch | Label='%s'"), *SuccessLogLabel);
		return true;
#elif PLATFORM_MAC
		const FString Executable = TEXT("/usr/bin/open");
		const FString Params = FString::Printf(TEXT("-a Terminal %s"), *CommandToRun);
		const bool bLaunchDetached = true;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessId = 0;

		UE_LOG(LogAuraEditor, Display, TEXT("Launching process | Label='%s' | Executable='%s' | Params='%s' | WorkingDir='%s' | Detached=%s | Hidden=%s | ReallyHidden=%s"),
			*SuccessLogLabel,
			*Executable,
			*Params,
			*FPaths::ProjectDir(),
			bLaunchDetached ? TEXT("true") : TEXT("false"),
			bLaunchHidden ? TEXT("true") : TEXT("false"),
			bLaunchReallyHidden ? TEXT("true") : TEXT("false"));

		FProcHandle ProcHandle = FPlatformProcess::CreateProc(
			*Executable,
			*Params,
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			&ProcessId,
			0,
			*FPaths::ProjectDir(),
			nullptr);

		if (!ProcHandle.IsValid())
		{
			UE_LOG(LogAuraEditor, Error, TEXT("CreateProc failed | Label='%s'"), *SuccessLogLabel);
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FailureDialogText));
			return false;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("CreateProc succeeded | Label='%s' | PID=%u"), *SuccessLogLabel, ProcessId);
		FPlatformProcess::CloseProc(ProcHandle);
		UE_LOG(LogAuraEditor, Display, TEXT("Process handle closed after successful launch | Label='%s'"), *SuccessLogLabel);
		return true;
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Launch blocked: non-Windows platform | Label='%s'"), *SuccessLogLabel);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnsupportedLaunchAction", "This action is not supported on this platform."));
		return false;
#endif
	}

	void OnStartDedicatedServerClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Dedicated server toolbar button clicked"));

#if PLATFORM_WINDOWS
		LaunchProjectScript(
			TEXT("StartDedicatedServer.bat"),
			FText::Format(
				LOCTEXT("StartDedicatedServerMissingWindows", "Could not find StartDedicatedServer.bat at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("StartDedicatedServer.bat")))),
			TEXT("Failed to launch StartDedicatedServer.bat."),
			TEXT("StartDedicatedServer"));
#elif PLATFORM_MAC
		LaunchProjectScript(
			TEXT("StartDedicatedServer.command"),
			FText::Format(
				LOCTEXT("StartDedicatedServerMissingMac", "Could not find StartDedicatedServer.command at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("StartDedicatedServer.command")))),
			TEXT("Failed to launch StartDedicatedServer.command."),
			TEXT("StartDedicatedServer"));
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Launch blocked: unsupported platform"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("StartDedicatedServerUnsupported", "Dedicated server launch is not supported on this platform."));
#endif
	}

	void OnBuildClientClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Build client menu option clicked"));

#if PLATFORM_WINDOWS
		const FString RunUATBatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat"));
		const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		const FString ArchiveDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Build/WindowsNoEditor"));

		UE_LOG(LogAuraEditor, Display, TEXT("Resolved RunUAT path: '%s'"), *RunUATBatPath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved project file path: '%s'"), *ProjectFilePath);
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved archive directory: '%s'"), *ArchiveDirectory);

		if (!FPaths::FileExists(RunUATBatPath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Build client aborted: RunUAT.bat does not exist"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("BuildWindowsClientMissingRunUATBat", "Could not find Unreal RunUAT.bat at:\n{0}"),
					FText::FromString(RunUATBatPath)));
			return;
		}

		if (!FPaths::FileExists(ProjectFilePath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Build client aborted: project file does not exist"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("BuildWindowsClientMissingProject", "Could not find the project file at:\n{0}"),
					FText::FromString(ProjectFilePath)));
			return;
		}

		const FString BuildCommand = FString::Printf(
			TEXT("\"%s\" BuildCookRun -project=\"%s\" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -package -archive -archivedirectory=\"%s\" -pak -iostore -prereqs -target=Aura"),
			*RunUATBatPath,
			*ProjectFilePath,
			*ArchiveDirectory);

		LaunchInVisibleConsole(
			BuildCommand,
			TEXT("Failed to launch the Windows Shipping client package build."),
			TEXT("BuildWindowsClient"));
#elif PLATFORM_MAC
		LaunchProjectScript(
			TEXT("BuildMacClient.command"),
			FText::Format(
				LOCTEXT("BuildMacClientMissingScript", "Could not find BuildMacClient.command at:\n{0}"),
				FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("BuildMacClient.command")))),
			TEXT("Failed to launch the Mac Shipping client package build."),
			TEXT("BuildMacClient"));
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Build client blocked: unsupported platform"));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BuildClientUnsupported", "Building the client from this menu is not supported on this platform."));
#endif
	}

	TSharedPtr<FExtender> ToolbarExtender;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAuraEditorModule, AuraEditor)
