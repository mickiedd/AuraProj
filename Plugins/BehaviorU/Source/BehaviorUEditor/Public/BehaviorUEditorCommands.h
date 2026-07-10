// BehaviorU UE5 Plugin - Editor Utilities
// Console commands for development workflow

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR

namespace BehaviorUEditorCommands
{
	/**
	 * Console command: BehaviorU.ReimportBT <AssetName>
	 * Reimports a behavior tree asset from its XML source
	 * Example: BehaviorU.ReimportBT BT_SimpleNPC
	 */
	static FAutoConsoleCommand ReimportBTCommand(
		TEXT("BehaviorU.ReimportBT"),
		TEXT("Reimport a Behavior Tree asset from XML. Usage: BehaviorU.ReimportBT <AssetName>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: BehaviorU.ReimportBT <AssetName>"));
				UE_LOG(LogTemp, Warning, TEXT("Example: BehaviorU.ReimportBT BT_SimpleNPC"));
				return;
			}

			FString AssetName = Args[0];
			FString AssetPath = FString::Printf(TEXT("/Game/AI/%s"), *AssetName);

			UE_LOG(LogTemp, Warning, TEXT("🔄 Reimporting Behavior Tree: %s"), *AssetName);
			
			// TODO: Implement actual reimport logic
			// For now, just log that we need to manually reimport
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Manual reimport required:"));
			UE_LOG(LogTemp, Warning, TEXT("   1. Right-click asset '%s' in Content Browser"), *AssetName);
			UE_LOG(LogTemp, Warning, TEXT("   2. Select 'Reimport'"));
			UE_LOG(LogTemp, Warning, TEXT("   Or delete .uasset and let it reimport on load"));
		})
	);

	/**
	 * Console command: BehaviorU.ReimportAllBT
	 * Reimports all behavior trees in /Game/AI/
	 */
	static FAutoConsoleCommand ReimportAllBTCommand(
		TEXT("BehaviorU.ReimportAllBT"),
		TEXT("Reimport all Behavior Tree assets from XML in /Game/AI/"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogTemp, Warning, TEXT("🔄 Reimporting all Behavior Trees in /Game/AI/"));
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Manual reimport required - see BehaviorU.ReimportBT for details"));
		})
	);

	/**
	 * Console command: BehaviorU.DeleteBTCache
	 * Deletes .uasset files to force reimport from XML
	 */
	static FAutoConsoleCommand DeleteBTCacheCommand(
		TEXT("BehaviorU.DeleteBTCache"),
		TEXT("Delete BT .uasset cache files to force fresh import. Usage: BehaviorU.DeleteBTCache <AssetName>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: BehaviorU.DeleteBTCache <AssetName>"));
				return;
			}

			FString AssetName = Args[0];
			FString ContentPath = FPaths::ProjectContentDir() / TEXT("AI");
			FString UAssetPath = ContentPath / (AssetName + TEXT(".uasset"));

			if (FPaths::FileExists(UAssetPath))
			{
				if (IFileManager::Get().Delete(*UAssetPath))
				{
					UE_LOG(LogTemp, Warning, TEXT("✅ Deleted cache: %s"), *UAssetPath);
					UE_LOG(LogTemp, Warning, TEXT("   Restart editor to reimport from XML"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("❌ Failed to delete: %s"), *UAssetPath);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("⚠️ File not found: %s"), *UAssetPath);
			}
		})
	);
}

#endif // WITH_EDITOR
