// AuraAbilityGraphEditorToolbarCommands.cpp

#include "AuraAbilityGraphEditorToolbarCommands.h"

#define LOCTEXT_NAMESPACE "FAuraAbilityGraphEditorModule"

void FAuraAbilityGraphEditorToolbarCommands::RegisterCommands()
{
    UI_COMMAND(
        OpenAbilityGraphEditor,
        "Ability Graph Editor",
        "Launch the AuraAbilityGraph web editor (AuraAbilityGraphLauncher.exe)",
        EUserInterfaceActionType::Button,
        FInputChord());
}

#undef LOCTEXT_NAMESPACE