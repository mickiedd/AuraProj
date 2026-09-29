#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CantonTerrainLibrary.generated.h"

class UMaterialInterface;

/** Editor-only import and validation for the isolated Canton modern-context prototype. */
UCLASS()
class AURAEDITOR_API UCantonTerrainLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool ConfigureDistrictNavigation(UWorld* World);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static UWorld* CreateProvisionalWorld();
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static UWorld* CreateDistrictWorld();
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool LoadProvisionalRegion(UWorld* World);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool ImportProvisionalTerrain(UWorld* World, const FString& RawHeightPath);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool AddDistrictEditLayers(UWorld* World);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static FString ValidateDistrictWorld(UWorld* World, const FString& RawHeightPath);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static bool ConfigureDistrictLandscapeMaterial(UWorld* World, UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable, Category="Canton|Editor")
    static FString ValidateProvisionalTerrain(UWorld* World, const FString& RawHeightPath);
};
