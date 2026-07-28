// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"

class SWidget;
class FUICommandList;

class FAuraAbilityGraphEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    // ── Ability Graph 工具条下拉菜单 ──────────────────────────────────────────
    void RegisterMenus();

    // 启动 AuraAbilityGraphLauncher.exe
    void OnOpenAbilityGraphEditor();

    // 下拉菜单 Widget（FOnGetContent）
    TSharedRef<SWidget> BuildAbilityGraphMenuWidget();

    /** Ability Graph 工具条命令列表 */
    TSharedPtr<FUICommandList> PluginCommands;
};