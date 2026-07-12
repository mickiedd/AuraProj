// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

using UnrealBuildTool;

public class AuraAutoTestRuntime : ModuleRules
{
	public AuraAutoTestRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// BehaviorURuntime must be public: our public headers (AutoTestAgent.h,
		// AutoTestNodes.h) include BehaviorU headers and subclass BehaviorU types.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"BehaviorURuntime",
			"JsonUtilities",
			"Projects",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"XmlParser",
		});

		bUseRTTI = false;
		bEnableExceptions = false;
	}
}