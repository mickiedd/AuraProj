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
		UE_LOG(LogAuraEditor, Display, TEXT("Legacy toolbar builder callback fired; adding button"));

		ToolbarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateRaw(this, &FAuraEditorModule::OnStartDedicatedServerClicked)),
			NAME_None,
			LOCTEXT("StartDedicatedServerLabel", "Dedicated Server"),
			LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.bat from the project root."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Play"));
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
				LOCTEXT("StartDedicatedServerTooltip", "Launch StartDedicatedServer.bat from the project root."),
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

	void OnStartDedicatedServerClicked() const
	{
		UE_LOG(LogAuraEditor, Display, TEXT("Dedicated server toolbar button clicked"));

#if PLATFORM_WINDOWS
		const FString BatPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("StartDedicatedServer.bat"));
		UE_LOG(LogAuraEditor, Display, TEXT("Resolved bat path: '%s'"), *BatPath);

		if (!FPaths::FileExists(BatPath))
		{
			UE_LOG(LogAuraEditor, Error, TEXT("Launch aborted: bat file does not exist"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("StartDedicatedServerMissing", "Could not find StartDedicatedServer.bat at:\n{0}"),
					FText::FromString(BatPath)));
			return;
		}

		const FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("ComSpec"));
		const FString Executable = CmdExe.IsEmpty() ? TEXT("cmd.exe") : CmdExe;
		// /k keeps the console window open so startup output remains visible.
		const FString Params = FString::Printf(TEXT("/k \"%s\""), *BatPath);
		const bool bLaunchDetached = false;
		const bool bLaunchHidden = false;
		const bool bLaunchReallyHidden = false;
		uint32 ProcessId = 0;
		UE_LOG(LogAuraEditor, Display, TEXT("Launching process | Executable='%s' | Params='%s' | WorkingDir='%s' | Detached=%s | Hidden=%s | ReallyHidden=%s"),
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
			UE_LOG(LogAuraEditor, Error, TEXT("CreateProc failed"));
			FMessageDialog::Open(
				EAppMsgType::Ok,
				LOCTEXT("StartDedicatedServerFailed", "Failed to launch StartDedicatedServer.bat."));
			return;
		}

		UE_LOG(LogAuraEditor, Display, TEXT("CreateProc succeeded | PID=%u"), ProcessId);

		FPlatformProcess::CloseProc(ProcHandle);
		UE_LOG(LogAuraEditor, Display, TEXT("Process handle closed after successful launch"));
#else
		UE_LOG(LogAuraEditor, Warning, TEXT("Launch blocked: non-Windows platform"));
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("StartDedicatedServerWindowsOnly", "StartDedicatedServer.bat launch is only supported on Windows."));
#endif
	}

	TSharedPtr<FExtender> ToolbarExtender;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAuraEditorModule, AuraEditor)
