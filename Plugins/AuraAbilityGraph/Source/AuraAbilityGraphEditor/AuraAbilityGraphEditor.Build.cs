// Copyright Druid Mechanics

using UnrealBuildTool;
using System.Collections.Generic;

public class AuraAbilityGraphEditor : ModuleRules
{
    public AuraAbilityGraphEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "AuraAbilityGraph"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "AssetTools",
            "EditorScriptingUtilities",
            "XmlParser"
        });
    }
}
