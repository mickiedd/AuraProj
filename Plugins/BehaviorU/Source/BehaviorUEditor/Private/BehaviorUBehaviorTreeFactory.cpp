// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUBehaviorTreeFactory.h"
#include "BehaviorTree/BehaviorUBehaviorTree.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// ===================================================================
// Create New Factory
// ===================================================================

UBehaviorUBehaviorTreeFactory::UBehaviorUBehaviorTreeFactory()
{
	SupportedClass = UBehaviorUBehaviorTree::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UBehaviorUBehaviorTreeFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UBehaviorUBehaviorTree* NewTree = NewObject<UBehaviorUBehaviorTree>(InParent, InClass, InName, Flags);
	NewTree->TreeName = InName.ToString();
	return NewTree;
}

FText UBehaviorUBehaviorTreeFactory::GetDisplayName() const
{
	return FText::FromString(TEXT("BehaviorU Behavior Tree"));
}

uint32 UBehaviorUBehaviorTreeFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

// ===================================================================
// Import Factory
// ===================================================================

UBehaviorUBehaviorTreeImportFactory::UBehaviorUBehaviorTreeImportFactory()
{
	SupportedClass = UBehaviorUBehaviorTree::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("xml;BehaviorU Behavior Tree XML"));
}

bool UBehaviorUBehaviorTreeImportFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".xml"));
}

UObject* UBehaviorUBehaviorTreeImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;
	return ImportBehaviorTree(InClass, InParent, InName, Flags, Filename, Warn);
}

UObject* UBehaviorUBehaviorTreeImportFactory::ImportBehaviorTree(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, FFeedbackContext* Warn)
{
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorU] ❌ Failed to read import file: %s"), *Filename);
		return nullptr;
	}

	UBehaviorUBehaviorTree* NewTree = NewObject<UBehaviorUBehaviorTree>(InParent, InClass, InName, Flags);
	NewTree->TreeName = InName.ToString();
	NewTree->SourceFilePath = Filename;

	if (!NewTree->LoadFromXML(FileContent))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorU] ❌ Failed to parse behavior tree from: %s"), *Filename);
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviorU] ✅ Successfully imported behavior tree: %s from %s"), *InName.ToString(), *Filename);
	return NewTree;
}

// ===================================================================
// Reimport Support
// ===================================================================

bool UBehaviorUBehaviorTreeImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UBehaviorUBehaviorTree* Tree = Cast<UBehaviorUBehaviorTree>(Obj);
	if (Tree && !Tree->SourceFilePath.IsEmpty())
	{
		OutFilenames.Add(Tree->SourceFilePath);
		return true;
	}
	return false;
}

void UBehaviorUBehaviorTreeImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UBehaviorUBehaviorTree* Tree = Cast<UBehaviorUBehaviorTree>(Obj);
	if (Tree && NewReimportPaths.Num() > 0)
	{
		Tree->SourceFilePath = NewReimportPaths[0];
	}
}

EReimportResult::Type UBehaviorUBehaviorTreeImportFactory::Reimport(UObject* Obj)
{
	UBehaviorUBehaviorTree* Tree = Cast<UBehaviorUBehaviorTree>(Obj);
	if (!Tree)
	{
		return EReimportResult::Failed;
	}

	if (Tree->SourceFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorU] ❌ Cannot reimport: No source file path stored"));
		return EReimportResult::Failed;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BehaviorU] 🔄 Reimporting behavior tree from: %s"), *Tree->SourceFilePath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Tree->SourceFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorU] ❌ Failed to read source file: %s"), *Tree->SourceFilePath);
		return EReimportResult::Failed;
	}

	if (!Tree->LoadFromXML(FileContent))
	{
		UE_LOG(LogTemp, Error, TEXT("[BehaviorU] ❌ Failed to parse behavior tree from: %s"), *Tree->SourceFilePath);
		return EReimportResult::Failed;
	}

	Tree->MarkPackageDirty();

	UE_LOG(LogTemp, Warning, TEXT("[BehaviorU] ✅ Successfully reimported behavior tree: %s"), *Tree->TreeName);

	return EReimportResult::Succeeded;
}
