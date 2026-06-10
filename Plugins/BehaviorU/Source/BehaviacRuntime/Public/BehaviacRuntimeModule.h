// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBehaviacRuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get this module */
	static FBehaviacRuntimeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FBehaviacRuntimeModule>("BehaviacRuntime");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("BehaviacRuntime");
	}
};
