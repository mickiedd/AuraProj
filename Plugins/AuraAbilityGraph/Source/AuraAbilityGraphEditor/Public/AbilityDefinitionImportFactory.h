// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "EditorReimportHandler.h"
#include "AbilityDefinitionImportFactory.generated.h"

UCLASS()
class AURAABILITYGRAPHEDITOR_API UAbilityDefinitionImportFactory : public UFactory, public FReimportHandler
{
    GENERATED_BODY()

public:
    UAbilityDefinitionImportFactory();

    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
    virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
    virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

    virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
    virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
    virtual EReimportResult::Type Reimport(UObject* Obj) override;

    virtual uint32 GetMenuCategories() const override;
    virtual FText GetDisplayName() const override;
    virtual bool FactoryCanImport(const FString& Filename) override;
};
