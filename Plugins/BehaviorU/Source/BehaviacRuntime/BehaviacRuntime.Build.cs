// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.


using UnrealBuildTool;

public class BehaviacRuntime : ModuleRules
{
	public BehaviacRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			// Public headers
		});

		PrivateIncludePaths.AddRange(new string[] {
			// Private headers
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"XmlParser",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
		});

		// Enable RTTI for dynamic casting if needed
		bUseRTTI = false;

		// Enable exceptions for XML parsing
		bEnableExceptions = false;
	}
}
