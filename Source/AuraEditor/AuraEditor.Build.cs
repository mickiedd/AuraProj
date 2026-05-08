// Copyright Druid Mechanics

using UnrealBuildTool;

public class AuraEditor : ModuleRules
{
	public AuraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"AssetRegistry",
			"ContentBrowser",
			"CoreUObject",
			"DesktopPlatform",
			"Engine",
			"Json",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"LevelEditor",
			"UnrealEd"
		});
	}
}
