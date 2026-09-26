#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CantonTerrainLibrary.generated.h"

/** Editor-only import and validation for the isolated Canton modern-context prototype. */
UCLASS()
class AURAEDITOR_API UCantonTerrainLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static UWorld* CreateProvisionalWorld();
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool LoadProvisionalRegion(UWorld* World);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool ImportProvisionalTerrain(UWorld* World, const FString& RawHeightPath);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static FString ValidateProvisionalTerrain(UWorld* World, const FString& RawHeightPath);
};
