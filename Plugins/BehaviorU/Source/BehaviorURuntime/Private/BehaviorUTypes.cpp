// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUTypes.h"

DEFINE_LOG_CATEGORY(LogBehaviorU);

TAutoConsoleVariable<int32> CVarBehaviorUVerboseLogging(
	TEXT("BehaviorU.VerboseLogging"),
	0,
	TEXT("Enable verbose BehaviorU debug logging.\n")
	TEXT("  0 = silent (default)\n")
	TEXT("  1 = verbose (initialization, tree loading, execution steps)"),
	ECVF_Default
);
