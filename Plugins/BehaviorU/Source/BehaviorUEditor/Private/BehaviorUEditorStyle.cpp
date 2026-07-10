// BehaviorUEditorStyle.cpp

#include "BehaviorUEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FBehaviorUEditorStyle::StyleInstance = nullptr;

void FBehaviorUEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBehaviorUEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBehaviorUEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BehaviorUEditorStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

const FVector2D Icon40x40(40.f, 40.f);

TSharedRef<FSlateStyleSet> FBehaviorUEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("BehaviorUEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BehaviorUPlugin")->GetBaseDir() / TEXT("Resources"));
	// 工具栏 Combo 按钮图标（行为树 + 下拉箭头）
	Style->Set("BehaviorUEditor.BehaviorUCombo",           new IMAGE_BRUSH(TEXT("BehaviorUComboIcon_40x"),        Icon40x40));
	// 下拉菜单各条目图标
	Style->Set("BehaviorUEditor.OpenBTEditor",            new IMAGE_BRUSH(TEXT("BehaviorUEditorIcon_40x"),       Icon40x40));
	Style->Set("BehaviorUEditor.ReimportSelectedBT",      new IMAGE_BRUSH(TEXT("BehaviorUReimportIcon_40x"),     Icon40x40));
	Style->Set("BehaviorUEditor.ToggleDebugServer",       new IMAGE_BRUSH(TEXT("BehaviorUDebugIcon_40x"),        Icon40x40));
	Style->Set("BehaviorUEditor.ToggleServerDebugServer", new IMAGE_BRUSH(TEXT("BehaviorUServerDebugIcon_40x"),  Icon40x40));

	return Style;
}

#undef IMAGE_BRUSH

void FBehaviorUEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FBehaviorUEditorStyle::Get()
{
	return *StyleInstance;
}
