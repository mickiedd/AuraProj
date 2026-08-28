// Copyright Druid Mechanics

using UnrealBuildTool;

public class Aura : ModuleRules
{
	public Aura(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "CoreOnline", "Engine", "NetCore", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "UMG", "OnlineSubsystemUtils", "OnlineSubsystem", "ModelViewViewModel", "AuraAbilityGraph", "AuraWebUI" });

		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTasks", "NavigationSystem", "Niagara", "AIModule", "Slate", "SlateCore", "Json", "JsonUtilities", "Sockets", "Networking", "ImageWrapper", "Projects" });
		
		// BehaviorU (BehaviorU) plugin — drives the test behavior tree bound to AuraEnemy.
		// AuraBehaviorUAgentComponent.h is a public header that includes BehaviorUAgent.h,
		// so BehaviorURuntime must be a public dependency.
		PublicDependencyModuleNames.Add("BehaviorURuntime");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
