// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviacRuntimeModule.h"
#include "BehaviacTypes.h"

#define LOCTEXT_NAMESPACE "FBehaviacRuntimeModule"

void FBehaviacRuntimeModule::StartupModule()
{
	UE_LOG(LogBehaviac, Log, TEXT("BehaviacRuntime module started. Version 1.0.0 (ported from behaviac 3.6.39)"));
}

void FBehaviacRuntimeModule::ShutdownModule()
{
	UE_LOG(LogBehaviac, Log, TEXT("BehaviacRuntime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBehaviacRuntimeModule, BehaviacRuntime)
