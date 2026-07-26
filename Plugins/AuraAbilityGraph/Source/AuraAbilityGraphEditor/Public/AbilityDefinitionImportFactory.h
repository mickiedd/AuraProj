// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AssetToolsModule.h"
#include "EditorReimportHandler.h"
#include "AbilityDefinitionImportFactory.generated.h"

UCLASS()
class AURAABILITYGRAPHEDITOR_API UAbilityDefinitionImportFactory : public UFactory, public FReimportHandler
{
    GENERATED_BODY()

public:
    UAbilityDefinitionImportFactory();

    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled);
    virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const TCHAR* InBuffer, FFeedbackContext* Warn, bool& bOutOperationCanceled);

    virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames);
    virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths);
    virtual EReimportResult::Type Reimport(UObject* Obj);

    virtual uint32 GetMenuCategories() const;
    virtual FText GetDisplayName() const;
    bool FactoryCanImport(const FString& Filename) const;
};
