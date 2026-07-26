// Copyright Druid Mechanics

#include "AbilityDefinitionImportFactory.h"
#include "AbilityDefinition.h"
#include "Misc/FileHelper.h"
#include "AssetToolsModule.h"

UAbilityDefinitionImportFactory::UAbilityDefinitionImportFactory()
{
    SupportedClass = UAuraAbilityDefinition::StaticClass();
    bCreateNew = false;
    bEditorImport = true;
    bText = true;

    Formats.Add(TEXT("xml;Aura Ability Definition XML"));
}

UObject* UAbilityDefinitionImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;
    return FactoryCreateText(InClass, InParent, InName, Flags, nullptr, Warn, bOutOperationCanceled);
}

UObject* UAbilityDefinitionImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const TCHAR* InBuffer, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    FString XMLContent;
    if (InBuffer)
    {
        XMLContent = InBuffer;
    }
    else
    {
        bOutOperationCanceled = true;
        return nullptr;
    }

    UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(InParent, InClass, InName, Flags);
    if (Definition->LoadFromXML(XMLContent))
    {
        return Definition;
    }

    bOutOperationCanceled = true;
    return nullptr;
}

bool UAbilityDefinitionImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    if (UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Obj))
    {
        if (!Definition->SourceXML.IsEmpty())
        {
            // For reimport, we rely on the source file path being stored separately.
            // In a full implementation, store SourceFilePath on the asset.
            return false;
        }
    }
    return false;
}

void UAbilityDefinitionImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
}

EReimportResult::Type UAbilityDefinitionImportFactory::Reimport(UObject* Obj)
{
    return EReimportResult::Failed;
}

uint32 UAbilityDefinitionImportFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Misc;
}

FText UAbilityDefinitionImportFactory::GetDisplayName() const
{
    return FText::FromString(TEXT("Aura Ability Definition"));
}

bool UAbilityDefinitionImportFactory::FactoryCanImport(const FString& Filename) const
{
    return Filename.EndsWith(TEXT(".xml"));
}
