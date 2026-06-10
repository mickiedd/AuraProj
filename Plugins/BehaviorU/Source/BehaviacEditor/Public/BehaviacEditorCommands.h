// Behaviac UE5 Plugin - Editor Utilities
// Console commands for development workflow

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR

namespace BehaviacEditorCommands
{
	/**
	 * Console command: Behaviac.ReimportBT <AssetName>
	 * Reimports a behavior tree asset from its XML source
	 * Example: Behaviac.ReimportBT BT_SimpleNPC
	 */
	static FAutoConsoleCommand ReimportBTCommand(
		TEXT("Behaviac.ReimportBT"),
		TEXT("Reimport a Behavior Tree asset from XML. Usage: Behaviac.ReimportBT <AssetName>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: Behaviac.ReimportBT <AssetName>"));
				UE_LOG(LogTemp, Warning, TEXT("Example: Behaviac.ReimportBT BT_SimpleNPC"));
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
	 * Console command: Behaviac.ReimportAllBT
	 * Reimports all behavior trees in /Game/AI/
	 */
	static FAutoConsoleCommand ReimportAllBTCommand(
		TEXT("Behaviac.ReimportAllBT"),
		TEXT("Reimport all Behavior Tree assets from XML in /Game/AI/"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogTemp, Warning, TEXT("🔄 Reimporting all Behavior Trees in /Game/AI/"));
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Manual reimport required - see Behaviac.ReimportBT for details"));
		})
	);

	/**
	 * Console command: Behaviac.DeleteBTCache
	 * Deletes .uasset files to force reimport from XML
	 */
	static FAutoConsoleCommand DeleteBTCacheCommand(
		TEXT("Behaviac.DeleteBTCache"),
		TEXT("Delete BT .uasset cache files to force fresh import. Usage: Behaviac.DeleteBTCache <AssetName>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: Behaviac.DeleteBTCache <AssetName>"));
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
