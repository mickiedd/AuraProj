// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAutoTestRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FAutoTestRuntimeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAutoTestRuntimeModule>("AuraAutoTestRuntime");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AuraAutoTestRuntime");
	}
};