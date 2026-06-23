// Copyright Druid Mechanics

using UnrealBuildTool;

public class Aura : ModuleRules
{
	public Aura(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "UMG", "OnlineSubsystemUtils" });

		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTags", "GameplayTasks", "NavigationSystem", "Niagara", "AIModule", "Slate", "SlateCore", "Json", "JsonUtilities", "Sockets", "Networking" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// BehaviorU (Behaviac) plugin — drives the test behavior tree bound to AuraEnemy.
		// AuraBehaviacAgentComponent.h is a public header that includes BehaviacAgent.h,
		// so BehaviacRuntime must be a public dependency.
		PublicDependencyModuleNames.Add("BehaviacRuntime");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
