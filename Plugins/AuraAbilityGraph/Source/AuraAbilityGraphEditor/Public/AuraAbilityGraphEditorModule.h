// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAuraAbilityGraphEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
