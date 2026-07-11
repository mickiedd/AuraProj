// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorURuntimeModule.h"
#include "BehaviorUTypes.h"

#define LOCTEXT_NAMESPACE "FBehaviorURuntimeModule"

void FBehaviorURuntimeModule::StartupModule()
{
	UE_LOG(LogBehaviorU, Log, TEXT("BehaviorURuntime module started. Version 1.0.0 (ported from behavioru 3.6.39)"));
}

void FBehaviorURuntimeModule::ShutdownModule()
{
	UE_LOG(LogBehaviorU, Log, TEXT("BehaviorURuntime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBehaviorURuntimeModule, BehaviorURuntime)
