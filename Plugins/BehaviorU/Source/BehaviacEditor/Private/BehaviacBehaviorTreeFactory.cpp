// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviacBehaviorTreeFactory.h"
#include "BehaviorTree/BehaviacBehaviorTree.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// ===================================================================
// Create New Factory
// ===================================================================

UBehaviacBehaviorTreeFactory::UBehaviacBehaviorTreeFactory()
{
	SupportedClass = UBehaviacBehaviorTree::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UBehaviacBehaviorTreeFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBehaviacBehaviorTree* NewTree = NewObject<UBehaviacBehaviorTree>(InParent, InClass, InName, Flags);
	NewTree->TreeName = InName.ToString();
	return NewTree;
}

FText UBehaviacBehaviorTreeFactory::GetDisplayName() const
{
	return FText::FromString(TEXT("Behaviac Behavior Tree"));
}

uint32 UBehaviacBehaviorTreeFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

// ===================================================================
// Import Factory
// ===================================================================

UBehaviacBehaviorTreeImportFactory::UBehaviacBehaviorTreeImportFactory()
{
	SupportedClass = UBehaviacBehaviorTree::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("xml;Behaviac Behavior Tree XML"));
}

bool UBehaviacBehaviorTreeImportFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".xml"));
}

UObject* UBehaviacBehaviorTreeImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;
	return ImportBehaviorTree(InClass, InParent, InName, Flags, Filename, Warn);
}

UObject* UBehaviacBehaviorTreeImportFactory::ImportBehaviorTree(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, FFeedbackContext* Warn)
{
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogTemp, Error, TEXT("[Behaviac] ❌ Failed to read import file: %s"), *Filename);
		return nullptr;
	}

	UBehaviacBehaviorTree* NewTree = NewObject<UBehaviacBehaviorTree>(InParent, InClass, InName, Flags);
	NewTree->TreeName = InName.ToString();
	NewTree->SourceFilePath = Filename;

	if (!NewTree->LoadFromXML(FileContent))
	{
		UE_LOG(LogTemp, Error, TEXT("[Behaviac] ❌ Failed to parse behavior tree from: %s"), *Filename);
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Behaviac] ✅ Successfully imported behavior tree: %s from %s"), *InName.ToString(), *Filename);
	return NewTree;
}

// ===================================================================
// Reimport Support
// ===================================================================

bool UBehaviacBehaviorTreeImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UBehaviacBehaviorTree* Tree = Cast<UBehaviacBehaviorTree>(Obj);
	if (Tree && !Tree->SourceFilePath.IsEmpty())
	{
		OutFilenames.Add(Tree->SourceFilePath);
		return true;
	}
	return false;
}

void UBehaviacBehaviorTreeImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UBehaviacBehaviorTree* Tree = Cast<UBehaviacBehaviorTree>(Obj);
	if (Tree && NewReimportPaths.Num() > 0)
	{
		Tree->SourceFilePath = NewReimportPaths[0];
	}
}

EReimportResult::Type UBehaviacBehaviorTreeImportFactory::Reimport(UObject* Obj)
{
	UBehaviacBehaviorTree* Tree = Cast<UBehaviacBehaviorTree>(Obj);
	if (!Tree)
	{
		return EReimportResult::Failed;
	}

	if (Tree->SourceFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Behaviac] ❌ Cannot reimport: No source file path stored"));
		return EReimportResult::Failed;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Behaviac] 🔄 Reimporting behavior tree from: %s"), *Tree->SourceFilePath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Tree->SourceFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[Behaviac] ❌ Failed to read source file: %s"), *Tree->SourceFilePath);
		return EReimportResult::Failed;
	}

	if (!Tree->LoadFromXML(FileContent))
	{
		UE_LOG(LogTemp, Error, TEXT("[Behaviac] ❌ Failed to parse behavior tree from: %s"), *Tree->SourceFilePath);
		return EReimportResult::Failed;
	}

	Tree->MarkPackageDirty();

	UE_LOG(LogTemp, Warning, TEXT("[Behaviac] ✅ Successfully reimported behavior tree: %s"), *Tree->TreeName);

	return EReimportResult::Succeeded;
}
