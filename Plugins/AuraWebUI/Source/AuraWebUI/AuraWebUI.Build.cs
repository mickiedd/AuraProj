// Copyright Druid Mechanics

using System.IO;
using UnrealBuildTool;

public class AuraWebUI : ModuleRules
{
	public AuraWebUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"WebBrowserWidget"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"Projects",
			"Sockets",
			"Networking"
		});

		if (Target.Type != TargetType.Server)
		{
			RuntimeDependencies.Add("$(PluginDir)/Content/WebUI/...", StagedFileType.NonUFS);
		}
	}
}
