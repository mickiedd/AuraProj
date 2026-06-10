// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BehaviacTypes.h"
#include "BehaviacBehaviorTree.generated.h"

class UBehaviacBehaviorNode;

/**
 * UBehaviacBehaviorTree: Data asset representing a behavior tree definition.
 *
 * This is the UE5 equivalent of the original behaviac BehaviorTree class.
 * It can be created in the editor or imported from XML/BSON files.
 */
UCLASS(BlueprintType)
class BEHAVIACRUNTIME_API UBehaviacBehaviorTree : public UDataAsset
{
	GENERATED_BODY()

public:
	UBehaviacBehaviorTree();

	/** The root node of this behavior tree */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|BehaviorTree")
	UBehaviacBehaviorNode* RootNode;

	/** Human-readable name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|BehaviorTree")
	FString TreeName;

	/** Source file path (for imported trees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|BehaviorTree")
	FString SourceFilePath;

	/** 原始 XML 内容缓存，由 LoadFromXML 写入，供调试服务器广播使用 */
	UPROPERTY()
	FString SourceXML;

	/** Version number from the source file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|BehaviorTree")
	int32 Version;

	/** Agent type this tree is designed for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|BehaviorTree")
	FString AgentType;

	/** Get the root node */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|BehaviorTree")
	UBehaviacBehaviorNode* GetRootNode() const { return RootNode; }

	/** Load from XML string */
	bool LoadFromXML(const FString& XMLContent);

#if WITH_EDITORONLY_DATA
	/** Description for editor display */
	UPROPERTY(EditAnywhere, Category = "Behaviac|BehaviorTree")
	FString Description;
#endif
};

/**
 * UBehaviacBehaviorTreeLibrary: Static library for behavior tree operations.
 */
UCLASS()
class BEHAVIACRUNTIME_API UBehaviacBehaviorTreeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Load a behavior tree from an XML file path */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|BehaviorTree")
	static UBehaviacBehaviorTree* LoadBehaviorTreeFromFile(UObject* WorldContext, const FString& FilePath);
};
