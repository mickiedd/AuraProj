// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.


using UnrealBuildTool;

public class BehaviacEditor : ModuleRules
{
	public BehaviacEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"BehaviacRuntime",
			"XmlParser",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"UnrealEd",
			"AssetTools",
			"ContentBrowser",
			"PropertyEditor",
			"ToolMenus",
			"LevelEditor",
			"InputCore",
			"Projects",
		});
	}
}
