// Copyright Druid Mechanics

using UnrealBuildTool;
using System.Collections.Generic;

public class AuraAbilityGraph : ModuleRules
{
    public AuraAbilityGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GameplayTasks",
            "XmlParser",
            "GameplayAbilities",
            "Aura"
        });

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Slate",
                "SlateCore",
                "UnrealEd",
                "AssetTools",
                "EditorScriptingUtilities"
            });
        }
    }
}
