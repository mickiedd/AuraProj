// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.


using UnrealBuildTool;

public class BehaviorUEditor : ModuleRules
{
	public BehaviorUEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"BehaviorURuntime",
			"XmlParser",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"Sockets",
			"Networking",
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
