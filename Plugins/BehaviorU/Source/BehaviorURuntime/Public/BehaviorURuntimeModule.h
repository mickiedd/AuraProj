// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBehaviorURuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get this module */
	static FBehaviorURuntimeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FBehaviorURuntimeModule>("BehaviorURuntime");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("BehaviorURuntime");
	}
};
