// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

class FJsonValue;

struct FWebUIConfigFileInfo
{
	FString RelativePath;
	int64 Bytes = 0;
	FString Version;
	FString ModifiedUtc;
	bool bValid = false;
	FString ValidationError;
};

/**
 * Safe, project-scoped access to the JSON files under Content/Config.
 *
 * The Web UI never receives an arbitrary native path. Every operation is
 * resolved from a relative .json path and checked to remain inside the
 * project's Content/Config directory before any file I/O occurs.
 */
class FWebUIConfigService final
{
public:
	static bool ListConfigFiles(TArray<FWebUIConfigFileInfo>& OutFiles, FString& OutError);

	static bool LoadConfigFile(
		const FString& RelativePath,
		FString& OutJson,
		FString& OutVersion,
		int64& OutBytes,
		FString& OutModifiedUtc,
		bool& bOutValid,
		FString& OutValidationError,
		FString& OutError);

	static bool SaveConfigFile(
		const FString& RelativePath,
		const FString& Json,
		const FString& ExpectedVersion,
		FString& OutVersion,
		int64& OutBytes,
		FString& OutModifiedUtc,
		FString& OutBackupPath,
		FString& OutError);

	static FString GetConfigDirectory();

private:
	static bool ResolveConfigPath(const FString& InRelativePath, FString& OutRelativePath, FString& OutAbsolutePath, FString& OutError);
	static bool ParseJson(const FString& Json, TSharedPtr<FJsonValue>& OutValue);
	static bool SerializePrettyJson(const TSharedPtr<FJsonValue>& Value, FString& OutJson);
	static FString ComputeVersion(const FString& Json);
	static FString GetModifiedUtc(const FString& AbsolutePath);
	static void FillMetadata(const FString& RelativePath, const FString& AbsolutePath, FWebUIConfigFileInfo& OutInfo);
};
