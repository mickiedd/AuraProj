// Copyright Druid Mechanics

#include "UI/Config/WebUIConfigService.h"

#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WebUIConfigServicePrivate
{
	constexpr int32 MaxConfigBytes = 8 * 1024 * 1024;

	FString NormalizeDirectory(const FString& InDirectory)
	{
		FString Directory = InDirectory;
		FPaths::NormalizeFilename(Directory);
		while (Directory.EndsWith(TEXT("/")))
		{
			Directory.LeftChopInline(1);
		}
		return Directory;
	}

	FString JoinPathSegments(const TArray<FString>& Segments)
	{
		FString Result;
		for (const FString& Segment : Segments)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT("/");
			}
			Result += Segment;
		}
		return Result;
	}
}

FString FWebUIConfigService::GetConfigDirectory()
{
	return WebUIConfigServicePrivate::NormalizeDirectory(
		FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"))));
}

bool FWebUIConfigService::ResolveConfigPath(
	const FString& InRelativePath,
	FString& OutRelativePath,
	FString& OutAbsolutePath,
	FString& OutError)
{
	FString Normalized = InRelativePath.TrimStartAndEnd();
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Normalized.StartsWith(TEXT("./")))
	{
		Normalized.RightChopInline(2);
	}

	if (Normalized.IsEmpty() || Normalized.StartsWith(TEXT("/")) || Normalized.Contains(TEXT(":")))
	{
		OutError = TEXT("Config paths must be relative .json paths inside Content/Config.");
		return false;
	}

	TArray<FString> Segments;
	Normalized.ParseIntoArray(Segments, TEXT("/"), true);
	if (Segments.IsEmpty())
	{
		OutError = TEXT("Config path is empty.");
		return false;
	}

	for (const FString& Segment : Segments)
	{
		if (Segment == TEXT(".")
			|| Segment == TEXT("..")
			|| Segment.Contains(TEXT("*"))
			|| Segment.Contains(TEXT("?"))
			|| Segment.Contains(TEXT(":")))
		{
			OutError = TEXT("Config path contains an invalid or traversal segment.");
			return false;
		}
	}

	OutRelativePath = WebUIConfigServicePrivate::JoinPathSegments(Segments);
	if (!FPaths::GetExtension(OutRelativePath).Equals(TEXT("json"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Only .json files can be edited by the config editor.");
		return false;
	}

	const FString ConfigDirectory = GetConfigDirectory();
	OutAbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ConfigDirectory, OutRelativePath));
	FPaths::NormalizeFilename(OutAbsolutePath);
	if (!OutAbsolutePath.StartsWith(ConfigDirectory + TEXT("/"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("Config path resolves outside Content/Config.");
		return false;
	}

	return true;
}

bool FWebUIConfigService::ParseJson(const FString& Json, TSharedPtr<FJsonValue>& OutValue)
{
	OutValue.Reset();
	const FString Trimmed = Json.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
	return FJsonSerializer::Deserialize(Reader, OutValue) && OutValue.IsValid();
}

bool FWebUIConfigService::SerializePrettyJson(const TSharedPtr<FJsonValue>& Value, FString& OutJson)
{
	if (!Value.IsValid())
	{
		return false;
	}

	OutJson.Reset();
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Value, TEXT(""), Writer))
	{
		return false;
	}
	Writer->Close();
	OutJson.TrimStartAndEndInline();
	OutJson += TEXT("\n");
	return true;
}

FString FWebUIConfigService::ComputeVersion(const FString& Json)
{
	return FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Json));
}

FString FWebUIConfigService::GetModifiedUtc(const FString& AbsolutePath)
{
	const FFileStatData StatData = IFileManager::Get().GetStatData(*AbsolutePath);
	return StatData.bIsValid ? StatData.ModificationTime.ToIso8601() : FString();
}

void FWebUIConfigService::FillMetadata(
	const FString& RelativePath,
	const FString& AbsolutePath,
	FWebUIConfigFileInfo& OutInfo)
{
	OutInfo = FWebUIConfigFileInfo();
	OutInfo.RelativePath = RelativePath;

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		OutInfo.ValidationError = TEXT("Unable to read the file.");
		return;
	}

	OutInfo.Bytes = Json.Len();
	OutInfo.Version = ComputeVersion(Json);
	OutInfo.ModifiedUtc = GetModifiedUtc(AbsolutePath);

	TSharedPtr<FJsonValue> RootValue;
	OutInfo.bValid = ParseJson(Json, RootValue);
	if (!OutInfo.bValid)
	{
		OutInfo.ValidationError = TEXT("Invalid JSON.");
	}
}

bool FWebUIConfigService::ListConfigFiles(TArray<FWebUIConfigFileInfo>& OutFiles, FString& OutError)
{
	OutFiles.Reset();
	OutError.Reset();

	const FString ConfigDirectory = GetConfigDirectory();
	if (!IFileManager::Get().DirectoryExists(*ConfigDirectory))
	{
		OutError = FString::Printf(TEXT("Config directory does not exist: %s"), *ConfigDirectory);
		return false;
	}

	TArray<FString> AbsoluteFiles;
	IFileManager::Get().FindFilesRecursive(AbsoluteFiles, *ConfigDirectory, TEXT("*.json"), true, false, false);
	AbsoluteFiles.Sort([](const FString& A, const FString& B)
	{
		return A < B;
	});

	for (FString AbsolutePath : AbsoluteFiles)
	{
		FPaths::NormalizeFilename(AbsolutePath);
		if (!AbsolutePath.StartsWith(ConfigDirectory + TEXT("/"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString RelativePath = AbsolutePath.Mid(ConfigDirectory.Len() + 1);
		FWebUIConfigFileInfo& Info = OutFiles.AddDefaulted_GetRef();
		FillMetadata(RelativePath, AbsolutePath, Info);
	}

	return true;
}

bool FWebUIConfigService::LoadConfigFile(
	const FString& RelativePath,
	FString& OutJson,
	FString& OutVersion,
	int64& OutBytes,
	FString& OutModifiedUtc,
	bool& bOutValid,
	FString& OutValidationError,
	FString& OutError)
{
	OutJson.Reset();
	OutVersion.Reset();
	OutBytes = 0;
	OutModifiedUtc.Reset();
	bOutValid = false;
	OutValidationError.Reset();
	OutError.Reset();

	FString NormalizedPath;
	FString AbsolutePath;
	if (!ResolveConfigPath(RelativePath, NormalizedPath, AbsolutePath, OutError))
	{
		return false;
	}
	if (!IFileManager::Get().FileExists(*AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Config file does not exist: %s"), *NormalizedPath);
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutJson, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Unable to read config file: %s"), *NormalizedPath);
		return false;
	}

	OutBytes = OutJson.Len();
	OutVersion = ComputeVersion(OutJson);
	OutModifiedUtc = GetModifiedUtc(AbsolutePath);
	TSharedPtr<FJsonValue> RootValue;
	bOutValid = ParseJson(OutJson, RootValue);
	if (!bOutValid)
	{
		OutValidationError = TEXT("Invalid JSON. Use the Raw JSON tab to repair it.");
	}
	return true;
}

bool FWebUIConfigService::SaveConfigFile(
	const FString& RelativePath,
	const FString& Json,
	const FString& ExpectedVersion,
	FString& OutVersion,
	int64& OutBytes,
	FString& OutModifiedUtc,
	FString& OutBackupPath,
	FString& OutError)
{
	OutVersion.Reset();
	OutBytes = 0;
	OutModifiedUtc.Reset();
	OutBackupPath.Reset();
	OutError.Reset();

	FString NormalizedPath;
	FString AbsolutePath;
	if (!ResolveConfigPath(RelativePath, NormalizedPath, AbsolutePath, OutError))
	{
		return false;
	}
	if (!IFileManager::Get().FileExists(*AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Config file does not exist: %s"), *NormalizedPath);
		return false;
	}
	if (Json.Len() > WebUIConfigServicePrivate::MaxConfigBytes)
	{
		OutError = TEXT("Config JSON exceeds the 8 MB editor limit.");
		return false;
	}

	FString CurrentJson;
	if (!FFileHelper::LoadFileToString(CurrentJson, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Unable to read the current config before saving: %s"), *NormalizedPath);
		return false;
	}
	const FString CurrentVersion = ComputeVersion(CurrentJson);
	if (ExpectedVersion.IsEmpty() || !ExpectedVersion.Equals(CurrentVersion, ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(
			TEXT("The file changed outside the editor. Reload it before saving. Current version is %s."),
			*CurrentVersion);
		return false;
	}

	TSharedPtr<FJsonValue> RootValue;
	if (!ParseJson(Json, RootValue))
	{
		OutError = TEXT("The editor submitted invalid JSON.");
		return false;
	}
	if (RootValue->Type != EJson::Object && RootValue->Type != EJson::Array)
	{
		OutError = TEXT("A config file must have an object or array at its root.");
		return false;
	}

	FString PrettyJson;
	if (!SerializePrettyJson(RootValue, PrettyJson))
	{
		OutError = TEXT("The editor could not serialize the JSON document.");
		return false;
	}

	const FString BackupPath = AbsolutePath + TEXT(".bak");
	if (!FFileHelper::SaveStringToFile(CurrentJson, *BackupPath))
	{
		OutError = TEXT("Could not create the safety backup; the original file was not changed.");
		return false;
	}

	const FString TempPath = AbsolutePath + TEXT(".tmp.") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!FFileHelper::SaveStringToFile(PrettyJson, *TempPath))
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		OutError = TEXT("Could not write the temporary config file; the original file was not changed.");
		return false;
	}

	if (!IFileManager::Get().Move(*AbsolutePath, *TempPath, true, true, false, true))
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		OutError = TEXT("Could not replace the config atomically; the original file was not changed.");
		return false;
	}

	OutVersion = ComputeVersion(PrettyJson);
	OutBytes = PrettyJson.Len();
	OutModifiedUtc = GetModifiedUtc(AbsolutePath);
	OutBackupPath = BackupPath;
	return true;
}
