// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "AutoTestRunContext.h"
#include "AutoTestResults.generated.h"

/** Discovered test metadata (one per XML file). */
struct FAutoTestInfo
{
	FString Name;        // display name (root <property name="name"> or filename)
	FString FilePath;    // absolute path to the XML
	FString Description;
	float Timeout = 60.0f;
	TArray<FString> Tags;
};

/** Result of a single test, collected by the runner and surfaced to the panel + JSON. */
USTRUCT(BlueprintType)
struct FAutoTestResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	FString Name;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	FString FilePath;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	EAutoTestStatus Status = EAutoTestStatus::NotRun;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	double DurationMs = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	TArray<FAutoTestAssertion> Assertions;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	FString Error;
};

/** A whole run's results. */
struct FAutoTestSuiteResult
{
	FString SuiteName = TEXT("AuraAutoTest");
	FDateTime Timestamp;
	double DurationMs = 0.0;

	int32 Total = 0;
	int32 Passed = 0;
	int32 Failed = 0;
	int32 TimedOut = 0;
	int32 Errored = 0;

	TArray<FAutoTestResult> Tests;
	FString ReportPath;
};