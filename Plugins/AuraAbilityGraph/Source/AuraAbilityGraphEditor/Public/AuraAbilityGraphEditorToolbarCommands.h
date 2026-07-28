// AuraAbilityGraphEditorToolbarCommands — UI_COMMAND for the Ability Graph toolbar dropdown.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "AuraAbilityGraphEditorStyle.h"

class FAuraAbilityGraphEditorToolbarCommands : public TCommands<FAuraAbilityGraphEditorToolbarCommands>
{
public:
    FAuraAbilityGraphEditorToolbarCommands()
        : TCommands<FAuraAbilityGraphEditorToolbarCommands>(
              TEXT("AuraAbilityGraphEditor"),
              NSLOCTEXT("Contexts", "AuraAbilityGraphEditor", "Aura Ability Graph Editor"),
              NAME_None,
              FAuraAbilityGraphEditorStyle::GetStyleSetName())
    {
    }

    void RegisterCommands() override;

    // 启动 AuraAbilityGraphLauncher.exe（Web 编辑器入口）
    TSharedPtr<FUICommandInfo> OpenAbilityGraphEditor;
};