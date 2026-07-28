// AuraAbilityGraphEditorStyle — Slate style set for the Ability Graph toolbar combo.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FAuraAbilityGraphEditorStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static void ReloadTextures();
    static FName GetStyleSetName();

private:
    static TSharedRef<FSlateStyleSet> Create();
    static TSharedPtr<FSlateStyleSet> StyleInstance;
};