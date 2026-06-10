// BehaviacEditorStyle.cpp

#include "BehaviacEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FBehaviacEditorStyle::StyleInstance = nullptr;

void FBehaviacEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBehaviacEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBehaviacEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BehaviacEditorStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

const FVector2D Icon40x40(40.f, 40.f);

TSharedRef<FSlateStyleSet> FBehaviacEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("BehaviacEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BehaviacPlugin")->GetBaseDir() / TEXT("Resources"));
	// 工具栏 Combo 按钮图标（行为树 + 下拉箭头）
	Style->Set("BehaviacEditor.BehaviacCombo",           new IMAGE_BRUSH(TEXT("BehaviacComboIcon_40x"),        Icon40x40));
	// 下拉菜单各条目图标
	Style->Set("BehaviacEditor.OpenBTEditor",            new IMAGE_BRUSH(TEXT("BehaviacEditorIcon_40x"),       Icon40x40));
	Style->Set("BehaviacEditor.ReimportSelectedBT",      new IMAGE_BRUSH(TEXT("BehaviacReimportIcon_40x"),     Icon40x40));
	Style->Set("BehaviacEditor.ToggleDebugServer",       new IMAGE_BRUSH(TEXT("BehaviacDebugIcon_40x"),        Icon40x40));
	Style->Set("BehaviacEditor.ToggleServerDebugServer", new IMAGE_BRUSH(TEXT("BehaviacServerDebugIcon_40x"),  Icon40x40));

	return Style;
}

#undef IMAGE_BRUSH

void FBehaviacEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FBehaviacEditorStyle::Get()
{
	return *StyleInstance;
}
