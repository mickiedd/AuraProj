// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "AutoTestResults.h"

/**
 * Writes a suite result to a JSON file under ProjectSavedDir/AutoTests/.
 * Returns the absolute path written to (empty on failure).
 */
FString AURAAUTOTESTRUNTIME_API WriteAutoTestReport(const FAutoTestSuiteResult& Suite);