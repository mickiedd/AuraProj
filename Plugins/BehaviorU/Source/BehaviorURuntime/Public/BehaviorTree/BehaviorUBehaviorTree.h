// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BehaviorUTypes.h"
#include "BehaviorUBehaviorTree.generated.h"

class UBehaviorUBehaviorNode;

/**
 * UBehaviorUBehaviorTree: Data asset representing a behavior tree definition.
 *
 * This is the UE5 equivalent of the original behavioru BehaviorTree class.
 * It can be created in the editor or imported from XML/BSON files.
 */
UCLASS(BlueprintType)
class BEHAVIORURUNTIME_API UBehaviorUBehaviorTree : public UDataAsset
{
	GENERATED_BODY()

public:
	UBehaviorUBehaviorTree();

	/** The root node of this behavior tree */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|BehaviorTree")
	UBehaviorUBehaviorNode* RootNode;

	/** Human-readable name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|BehaviorTree")
	FString TreeName;

	/** Source file path (for imported trees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|BehaviorTree")
	FString SourceFilePath;

	/** 原始 XML 内容缓存，由 LoadFromXML 写入，供调试服务器广播使用 */
	UPROPERTY()
	FString SourceXML;

	/** Version number from the source file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|BehaviorTree")
	int32 Version;

	/** Agent type this tree is designed for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|BehaviorTree")
	FString AgentType;

	/** Get the root node */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|BehaviorTree")
	UBehaviorUBehaviorNode* GetRootNode() const { return RootNode; }

	/** Load from XML string */
	bool LoadFromXML(const FString& XMLContent);

#if WITH_EDITORONLY_DATA
	/** Description for editor display */
	UPROPERTY(EditAnywhere, Category = "BehaviorU|BehaviorTree")
	FString Description;
#endif
};

/**
 * UBehaviorUBehaviorTreeLibrary: Static library for behavior tree operations.
 */
UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUBehaviorTreeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Load a behavior tree from an XML file path */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|BehaviorTree")
	static UBehaviorUBehaviorTree* LoadBehaviorTreeFromFile(UObject* WorldContext, const FString& FilePath);
};
