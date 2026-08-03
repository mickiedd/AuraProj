// Copyright Druid Mechanics

#include "AbilityDefinitionImportFactory.h"
#include "AbilityDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetToolsModule.h"

static UObject* CreateAbilityDefinitionFromXML(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& XMLContent, const FString& SourceFilePath, bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;
    if (XMLContent.IsEmpty())
    {
        bOutOperationCanceled = true;
        return nullptr;
    }

    UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(InParent, InClass ? InClass : UAuraAbilityDefinition::StaticClass(), InName, Flags);
    if (!Definition || !Definition->LoadFromXML(XMLContent))
    {
        bOutOperationCanceled = true;
        return nullptr;
    }

    Definition->SourceFilePath = SourceFilePath;
    return Definition;
}

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
    FString XMLContent;
    if (!FFileHelper::LoadFileToString(XMLContent, *Filename))
    {
        bOutOperationCanceled = true;
        return nullptr;
    }

    return CreateAbilityDefinitionFromXML(
        InClass,
        InParent,
        InName,
        Flags,
        XMLContent,
        FPaths::ConvertRelativePathToFull(Filename),
        bOutOperationCanceled);
}

UObject* UAbilityDefinitionImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
    FString XMLContent;
    if (Buffer && BufferEnd && BufferEnd >= Buffer)
    {
        XMLContent.AppendChars(Buffer, UE_PTRDIFF_TO_INT32(BufferEnd - Buffer));
    }
    else
    {
        return nullptr;
    }

    bool bIgnoredOperationCanceled = false;
    return CreateAbilityDefinitionFromXML(InClass, InParent, InName, Flags, XMLContent, FString(), bIgnoredOperationCanceled);
}

UObject* UAbilityDefinitionImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;
    if (!Buffer || !BufferEnd || BufferEnd < Buffer)
    {
        bOutOperationCanceled = true;
        return nullptr;
    }

    FString XMLContent;
    XMLContent.AppendChars(Buffer, UE_PTRDIFF_TO_INT32(BufferEnd - Buffer));
    return CreateAbilityDefinitionFromXML(InClass, InParent, InName, Flags, XMLContent, FString(), bOutOperationCanceled);
}

bool UAbilityDefinitionImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    if (UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Obj))
    {
        if (!Definition->SourceFilePath.IsEmpty())
        {
            OutFilenames.Add(Definition->SourceFilePath);
            return true;
        }
    }
    return false;
}

void UAbilityDefinitionImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
    if (UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Obj))
    {
        if (NewReimportPaths.Num() > 0)
        {
            Definition->SourceFilePath = FPaths::ConvertRelativePathToFull(NewReimportPaths[0]);
        }
    }
}

EReimportResult::Type UAbilityDefinitionImportFactory::Reimport(UObject* Obj)
{
    UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Obj);
    if (!Definition || Definition->SourceFilePath.IsEmpty())
    {
        return EReimportResult::Failed;
    }

    FString XMLContent;
    if (!FFileHelper::LoadFileToString(XMLContent, *Definition->SourceFilePath))
    {
        return EReimportResult::Failed;
    }

    Definition->Modify();
    if (!Definition->LoadFromXML(XMLContent))
    {
        return EReimportResult::Failed;
    }

    Definition->SourceFilePath = FPaths::ConvertRelativePathToFull(Definition->SourceFilePath);
    Definition->MarkPackageDirty();
    return EReimportResult::Succeeded;
}

uint32 UAbilityDefinitionImportFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Misc;
}

FText UAbilityDefinitionImportFactory::GetDisplayName() const
{
    return FText::FromString(TEXT("Aura Ability Definition"));
}

bool UAbilityDefinitionImportFactory::FactoryCanImport(const FString& Filename)
{
    return Filename.EndsWith(TEXT(".xml"));
}
