// Copyright Druid Mechanics

#include "AuraAbilityGraphEditorModule.h"
#include "AuraAbilityGraphEditorStyle.h"
#include "AuraAbilityGraphEditorToolbarCommands.h"

#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FAuraAbilityGraphEditorModule"

void FAuraAbilityGraphEditorModule::StartupModule()
{
    FAuraAbilityGraphEditorStyle::Initialize();
    FAuraAbilityGraphEditorStyle::ReloadTextures();

    // ── Ability Graph 命令注册 ─────────────────────────────────────────────────
    FAuraAbilityGraphEditorToolbarCommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);
    PluginCommands->MapAction(
        FAuraAbilityGraphEditorToolbarCommands::Get().OpenAbilityGraphEditor,
        FExecuteAction::CreateRaw(this, &FAuraAbilityGraphEditorModule::OnOpenAbilityGraphEditor),
        FCanExecuteAction());

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAuraAbilityGraphEditorModule::RegisterMenus));
}

void FAuraAbilityGraphEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FAuraAbilityGraphEditorToolbarCommands::Unregister();
    FAuraAbilityGraphEditorStyle::Shutdown();
}

void FAuraAbilityGraphEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // UE5 的 Level Editor 工具栏已迁移到 UToolMenus，直接扩展 PlayToolBar
    // （与 BehaviorU 插件一致的挂载点）。
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(
        "LevelEditor.LevelEditorToolBar.PlayToolBar");
    if (!ToolbarMenu)
    {
        UE_LOG(LogTemp, Error, TEXT("[AuraAbilityGraphEditor] Failed to extend LevelEditor PlayToolBar menu."));
        return;
    }

    FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("AuraAbilityGraphEditor"));

    FToolMenuEntry ComboEntry = FToolMenuEntry::InitComboButton(
        "AuraAbilityGraphCombo",
        FUIAction(),
        FOnGetContent::CreateRaw(this, &FAuraAbilityGraphEditorModule::BuildAbilityGraphMenuWidget),
        LOCTEXT("AuraAbilityGraphComboLabel",   "AbilityGraph"),
        LOCTEXT("AuraAbilityGraphComboTooltip", "Aura Ability Graph 工具"),
        FSlateIcon(FAuraAbilityGraphEditorStyle::GetStyleSetName(), "AuraAbilityGraphEditor.AbilityGraphCombo"),
        /*bInSimpleComboBox=*/false);

    Section.AddEntry(ComboEntry);
}

void FAuraAbilityGraphEditorModule::OnOpenAbilityGraphEditor()
{
    const FString LauncherPath = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / TEXT("Plugins/AuraAbilityGraph/AuraAbilityGraphLauncher/AuraAbilityGraphLauncher.exe"));

    if (!FPaths::FileExists(LauncherPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[AuraAbilityGraphEditor] AuraAbilityGraphLauncher.exe not found: %s"), *LauncherPath);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[AuraAbilityGraphEditor] Launching AuraAbilityGraphLauncher: %s"), *LauncherPath);
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

TSharedRef<SWidget> FAuraAbilityGraphEditorModule::BuildAbilityGraphMenuWidget()
{
    // Legacy Slate 路径：用 FMenuBuilder 构造下拉菜单
    // 第一个条目即为 AuraAbilityGraphLauncher.exe 的入口。
    FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, PluginCommands);

    MenuBuilder.BeginSection("AuraAbilityGraphEditorTools", LOCTEXT("AuraAbilityGraphEditorSectionLabel", "编辑器"));
    MenuBuilder.AddMenuEntry(FAuraAbilityGraphEditorToolbarCommands::Get().OpenAbilityGraphEditor);
    MenuBuilder.EndSection();

    return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAuraAbilityGraphEditorModule, AuraAbilityGraphEditor)