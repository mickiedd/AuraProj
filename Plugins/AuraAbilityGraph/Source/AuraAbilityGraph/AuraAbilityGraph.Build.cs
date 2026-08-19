// Copyright Druid Mechanics

using UnrealBuildTool;
using System.Collections.Generic;

public class AuraAbilityGraph : ModuleRules
{
    public AuraAbilityGraph(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        // The graph executes Aura gameplay abilities and Aura consumes graph
        // definitions.  This is an intentional runtime module cycle; declare
        // it so UBT can order/load the two modules as a supported pair instead
        // of treating the dependency as an accidental build-graph loop.
        CircularlyReferencedDependentModules.Add("Aura");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GameplayTasks",
            "XmlParser",
            "GameplayAbilities",
            "Niagara",
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
