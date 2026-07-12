// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

using UnrealBuildTool;

public class AuraAutoTestEditor : ModuleRules
{
	public AuraAutoTestEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Our runtime module is a public dependency: the editor module drives the
		// runner subsystem and surfaces its types in the Slate results panel.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AuraAutoTestRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"LevelEditor",
			"EditorStyle",
			"PropertyEditor",
			"InputCore",
			"Projects",
		});
	}
}