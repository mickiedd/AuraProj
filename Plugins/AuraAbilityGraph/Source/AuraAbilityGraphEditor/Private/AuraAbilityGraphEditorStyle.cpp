// AuraAbilityGraphEditorStyle.cpp

#include "AuraAbilityGraphEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FAuraAbilityGraphEditorStyle::StyleInstance = nullptr;

void FAuraAbilityGraphEditorStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FAuraAbilityGraphEditorStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FAuraAbilityGraphEditorStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("AuraAbilityGraphEditorStyle"));
    return StyleSetName;
}

TSharedRef<FSlateStyleSet> FAuraAbilityGraphEditorStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("AuraAbilityGraph")->GetBaseDir() / TEXT("Resources"));

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
    const FVector2D Icon40x40(40.f, 40.f);

    // 工具栏 Combo 按钮图标（节点图 + 下拉箭头）
    Style->Set("AuraAbilityGraphEditor.AbilityGraphCombo",
        new IMAGE_BRUSH(TEXT("AbilityGraphComboIcon_40x"), Icon40x40));
    // 下拉菜单条目图标（由 TCommands 框架按 {ContextName}.{CommandName} 查找）
    Style->Set("AuraAbilityGraphEditor.OpenAbilityGraphEditor",
        new IMAGE_BRUSH(TEXT("AbilityGraphComboIcon_40x"), Icon40x40));

#undef IMAGE_BRUSH
    return Style;
}

void FAuraAbilityGraphEditorStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}