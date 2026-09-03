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
			"AnimationCore",
			"AnimGraph",
			"AssetRegistry",
			"Aura",
			"ContentBrowser",
			"CoreUObject",
			"DesktopPlatform",
			"Engine",
			"Json",
			"GameplayAbilities",
			"GameplayTags",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"LevelEditor",
			"NavigationSystem",
			"UnrealEd"
		});
	}
}
