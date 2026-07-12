// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestRuntimeModule.h"
#include "AutoTestLog.h"
#include "AutoTestNodes.h"
#include "BehaviorUNodeRegistry.h"

#define LOCTEXT_NAMESPACE "FAutoTestRuntimeModule"

void FAutoTestRuntimeModule::StartupModule()
{
	// Register custom test nodes with BehaviorU's XML loader so <node class="..."> tags
	// resolve to our node classes. BehaviorURuntime loads before this module (we depend
	// on it publicly), so the registry exists here.
	FBehaviorUNodeRegistry::Get().Register(TEXT("Assert"),         [](UObject* O){ return NewObject<UAutoTestAssertNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("Expect"),          [](UObject* O){ return NewObject<UAutoTestExpectNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("WaitForProperty"), [](UObject* O){ return NewObject<UAutoTestWaitForPropertyNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("SpawnActor"),      [](UObject* O){ return NewObject<UAutoTestSpawnActorNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("LogResult"),       [](UObject* O){ return NewObject<UAutoTestLogResultNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("Pass"),            [](UObject* O){ return NewObject<UAutoTestPassNode>(O); });
	FBehaviorUNodeRegistry::Get().Register(TEXT("Fail"),            [](UObject* O){ return NewObject<UAutoTestFailNode>(O); });

	UE_LOG(LogAuraTest, Log, TEXT("AuraAutoTestRuntime module started. Registered 7 BehaviorU test nodes."));
}

void FAutoTestRuntimeModule::ShutdownModule()
{
	UE_LOG(LogAuraTest, Log, TEXT("AuraAutoTestRuntime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAutoTestRuntimeModule, AuraAutoTestRuntime)