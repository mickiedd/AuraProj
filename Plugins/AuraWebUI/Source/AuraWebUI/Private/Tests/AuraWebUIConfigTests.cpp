// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/Config/WebUIConfigService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebUIConfigServiceTest,
	"AuraWebUI.Plugin.ConfigService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebUIConfigServiceTest::RunTest(const FString& Parameters)
{
	TArray<FWebUIConfigFileInfo> Files;
	FString Error;
	if (!TestTrue(TEXT("Content/Config can be enumerated"), FWebUIConfigService::ListConfigFiles(Files, Error)))
	{
		AddError(Error);
		return false;
	}

	TestTrue(TEXT("The project exposes multiple editable JSON configs"), Files.Num() >= 10);
	const FWebUIConfigFileInfo* RoleConfig = Files.FindByPredicate([](const FWebUIConfigFileInfo& File)
	{
		return File.RelativePath == TEXT("RoleConfig.json");
	});
	TestNotNull(TEXT("RoleConfig.json is discoverable"), RoleConfig);
	if (RoleConfig)
	{
		TestTrue(TEXT("RoleConfig.json is valid JSON"), RoleConfig->bValid);
		TestFalse(TEXT("RoleConfig.json has a non-empty content version"), RoleConfig->Version.IsEmpty());
	}

	FString Json;
	FString Version;
	FString ModifiedUtc;
	FString ValidationError;
	int64 Bytes = 0;
	bool bValid = false;
	Error.Reset();
	if (!TestTrue(
		TEXT("A representative config can be loaded"),
		FWebUIConfigService::LoadConfigFile(TEXT("RoleConfig.json"), Json, Version, Bytes, ModifiedUtc, bValid, ValidationError, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Loaded representative config parses as JSON"), bValid);
	TestTrue(TEXT("Loaded representative config has content"), Bytes > 0 && Json.Contains(TEXT("roles")));

	FString IgnoredJson;
	FString IgnoredVersion;
	FString IgnoredModifiedUtc;
	FString IgnoredValidationError;
	int64 IgnoredBytes = 0;
	bool bIgnoredValid = false;
	TestFalse(
		TEXT("Traversal outside Content/Config is rejected"),
		FWebUIConfigService::LoadConfigFile(
			TEXT("../Aura.uproject"),
			IgnoredJson,
			IgnoredVersion,
			IgnoredBytes,
			IgnoredModifiedUtc,
			bIgnoredValid,
			IgnoredValidationError,
			Error));

	FString NewVersion;
	FString NewModifiedUtc;
	FString BackupPath;
	FString SaveError;
	int64 NewBytes = 0;
	TestFalse(
		TEXT("Invalid JSON is rejected before any write"),
		FWebUIConfigService::SaveConfigFile(
			TEXT("RoleConfig.json"),
			TEXT("{ invalid"),
			Version,
			NewVersion,
			NewBytes,
			NewModifiedUtc,
			BackupPath,
			SaveError));
	TestFalse(
		TEXT("A stale content version is rejected before any write"),
		FWebUIConfigService::SaveConfigFile(
			TEXT("RoleConfig.json"),
			Json,
			TEXT("STALE_VERSION"),
			NewVersion,
			NewBytes,
			NewModifiedUtc,
			BackupPath,
			SaveError));

	return true;
}

#endif
