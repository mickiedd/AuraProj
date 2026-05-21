// Copyright Druid Mechanics

using UnrealBuildTool;
using System.Collections.Generic;

public class AuraServerTarget : TargetRules
{
	public AuraServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange(new string[] { "Aura" });
	}
}
