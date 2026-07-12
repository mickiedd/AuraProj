// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestReportWriter.h"
#include "AutoTestLog.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"

static FString StatusToString(EAutoTestStatus S)
{
	switch (S)
	{
	case EAutoTestStatus::NotRun:	return TEXT("notrun");
	case EAutoTestStatus::Running:	return TEXT("running");
	case EAutoTestStatus::Pass:		return TEXT("pass");
	case EAutoTestStatus::Fail:		return TEXT("fail");
	case EAutoTestStatus::Timeout:	return TEXT("timeout");
	case EAutoTestStatus::Error:	return TEXT("error");
	}
	return TEXT("unknown");
}

FString WriteAutoTestReport(const FAutoTestSuiteResult& Suite)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("suite"), Suite.SuiteName);
	Writer->WriteValue(TEXT("timestamp"), Suite.Timestamp.ToIso8601());
	Writer->WriteValue(TEXT("duration_ms"), Suite.DurationMs);

	Writer->WriteObjectStart(TEXT("summary"));
	Writer->WriteValue(TEXT("total"), Suite.Total);
	Writer->WriteValue(TEXT("passed"), Suite.Passed);
	Writer->WriteValue(TEXT("failed"), Suite.Failed);
	Writer->WriteValue(TEXT("timeout"), Suite.TimedOut);
	Writer->WriteValue(TEXT("error"), Suite.Errored);
	Writer->WriteObjectEnd();

	Writer->WriteArrayStart(TEXT("tests"));
	for (const FAutoTestResult& Test : Suite.Tests)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Test.Name);
		Writer->WriteValue(TEXT("file"), Test.FilePath);
		Writer->WriteValue(TEXT("status"), StatusToString(Test.Status));
		Writer->WriteValue(TEXT("duration_ms"), Test.DurationMs);

		Writer->WriteArrayStart(TEXT("assertions"));
		for (const FAutoTestAssertion& A : Test.Assertions)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("message"), A.Message);
			Writer->WriteValue(TEXT("passed"), A.bPassed);
			Writer->WriteValue(TEXT("time"), A.Timestamp);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		if (Test.Error.IsEmpty())
		{
			Writer->WriteNull(TEXT("error"));
		}
		else
		{
			Writer->WriteValue(TEXT("error"), Test.Error);
		}
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();

	Writer->Close();

	if (Output.IsEmpty())
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Failed to serialize JSON report"));
		return FString();
	}

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AutoTests");
	IFileManager::Get().MakeDirectory(*Dir, /*bRecursive=*/true);

	const FString Timestamp = Suite.Timestamp.ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Path = Dir / FString::Printf(TEXT("Report_%s.json"), *Timestamp);

	if (!FFileHelper::SaveStringToFile(Output, *Path))
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Failed to write report to '%s'"), *Path);
		return FString();
	}

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Report written to %s"), *Path);
	return Path;
}