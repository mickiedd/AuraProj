// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviacTypes.h"

DEFINE_LOG_CATEGORY(LogBehaviac);

TAutoConsoleVariable<int32> CVarBehaviacVerboseLogging(
	TEXT("Behaviac.VerboseLogging"),
	0,
	TEXT("Enable verbose Behaviac debug logging.\n")
	TEXT("  0 = silent (default)\n")
	TEXT("  1 = verbose (initialization, tree loading, execution steps)"),
	ECVF_Default
);
