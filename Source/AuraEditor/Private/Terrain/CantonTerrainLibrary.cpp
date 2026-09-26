#include "Terrain/CantonTerrainLibrary.h"
#include "Landscape.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "WorldPartition/WorldPartition.h"

namespace CantonTerrain
{
    constexpr int32 VertexCount = 2017;
    bool ReadHeights(const FString& Path, TArray<uint16>& Heights)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != VertexCount * VertexCount * 2) return false;
        Heights.SetNumUninitialized(VertexCount * VertexCount);
        // Explicit little-endian decoder. The file is already south-first for UE +Y=north.
        for (int32 I = 0; I < Heights.Num(); ++I) Heights[I] = Bytes[2*I] | (uint16(Bytes[2*I+1]) << 8);
        return true;
    }
}

UWorld* UCantonTerrainLibrary::CreateProvisionalWorld()
{
    const FString Path=TEXT("/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL");
    if (FPackageName::DoesPackageExist(Path)) return nullptr;
    UWorld* World=GEditor->NewMap(true);
    if (!World || !UEditorLoadingAndSavingUtils::SaveMap(World,Path)) return nullptr;
    return World;
}

bool UCantonTerrainLibrary::LoadProvisionalRegion(UWorld* World)
{
    if (!World || !World->GetWorldPartition() || !World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/Provisional/"))) return false;
    World->GetWorldPartition()->LoadLastLoadedRegions({FBox(FVector(-1000,-1000,-20000),FVector(404200,404200,20000))});
    return true;
}

bool UCantonTerrainLibrary::ImportProvisionalTerrain(UWorld* World, const FString& RawHeightPath)
{
    using namespace CantonTerrain;
    if (!World || !World->GetWorldPartition() || !World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/Provisional/"))) return false;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It) return false; // Never overwrite a Landscape.
    TArray<uint16> Heights;
    if (!ReadHeights(RawHeightPath, Heights)) return false;
    ALandscape* Landscape = World->SpawnActor<ALandscape>();
    Landscape->SetActorLabel(TEXT("Canton_PROVISIONAL_Modern_Context"));
    Landscape->Tags.Add(TEXT("Canton.Provisional.ModernContextOnly"));
    Landscape->bCanHaveLayersContent = true;
    Landscape->SetActorScale3D(FVector(200, 200, 50));
    Landscape->SetActorLocation(FVector::ZeroVector);
    TMap<FGuid, TArray<uint16>> HeightData;
    HeightData.Add(FGuid(), MoveTemp(Heights));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> Materials;
    Materials.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
    Landscape->Import(FGuid::NewGuid(), 0, 0, VertexCount-1, VertexCount-1, 2, 63, HeightData,
        *RawHeightPath, Materials, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
    if (Landscape->GetLayerCount() != 1) return false;
    Landscape->SetLayerName(0, TEXT("Base_Imported"));
    Landscape->SetLayerLocked(0, true);
    ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
    if (!Info) return false;
    Info->RegionSizeInComponents = 16;
    World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Info, 4);
    Landscape->ForceLayersFullUpdate();
    Landscape->MarkPackageDirty();
    return true;
}

FString UCantonTerrainLibrary::ValidateProvisionalTerrain(UWorld* World, const FString& RawHeightPath)
{
    using namespace CantonTerrain;
    TArray<FString> Errors;
    auto Require = [&Errors](bool Good, const TCHAR* Message) { if (!Good) Errors.Add(Message); };
    Require(World && World->GetWorldPartition(), TEXT("World Partition missing"));
    TArray<uint16> Heights;
    Require(ReadHeights(RawHeightPath, Heights), TEXT("Invalid reference R16 file"));
    ALandscape* Landscape = nullptr;
    int32 LandscapeCount=0, ComponentCount=0, CollisionCount=0, ProxyCount=0, SampleCount=0;
    double MaxCollisionErrorCm=0;
    if (World)
    {
        for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
        {
            ++ProxyCount;
            if (ALandscape* L = Cast<ALandscape>(*It)) { Landscape=L; ++LandscapeCount; }
            Require(It->LandscapeMaterial && It->LandscapeMaterial->GetPathName().Contains(TEXT("/Game/Canton/Provisional/Terrain/MI_Diagnostic_PROVISIONAL")), TEXT("Diagnostic material missing"));
            Require(It->LandscapeComponents.Num()==0 || It->LandscapeComponents.Num()==16, TEXT("Proxy grid must be 4x4 components"));
            ComponentCount += It->LandscapeComponents.Num();
            CollisionCount += It->CollisionComponents.Num();
            for (ULandscapeComponent* C : It->LandscapeComponents)
                Require(C && C->NumSubsections==2 && C->SubsectionSizeQuads==63, TEXT("Incorrect component geometry"));
        }
    }
    Require(LandscapeCount==1, TEXT("Expected one main Landscape"));
    Require(ProxyCount==17, TEXT("Expected main Landscape plus sixteen 4x4-component proxies"));
    Require(World && World->GetWorldPartition() && World->GetWorldPartition()->IsStreamingEnabled(), TEXT("World Partition streaming disabled"));
    Require(ComponentCount==256, TEXT("Expected 256 loaded components"));
    Require(CollisionCount==256, TEXT("Expected 256 collision components"));
    if (Landscape)
    {
        Require(Landscape->GetActorScale3D().Equals(FVector(200,200,50), .001), TEXT("Incorrect actor scale"));
        Require(Landscape->GetActorLocation().IsNearlyZero(.001), TEXT("Incorrect actor origin"));
        Require(Landscape->GetActorRotation().IsNearlyZero(.001), TEXT("Incorrect actor rotation"));
        Require(Landscape->bCanHaveLayersContent, TEXT("Edit layers disabled"));
        const FLandscapeLayer* Layer=Landscape->GetLayerConst(FName(TEXT("Base_Imported")));
        Require(Layer && Layer->bLocked, TEXT("Base_Imported not locked"));
        ULandscapeInfo* Info=Landscape->GetLandscapeInfo();
        Require(Info != nullptr, TEXT("Landscape info missing"));
        if (Info && Heights.Num()==VertexCount*VertexCount)
        {
            FLandscapeEditDataInterface Edit(Info);
            if (Layer) Edit.SetEditLayer(Layer->Guid);
            // Asymmetric pixel checks detect flips/transposes. Includes gate district and corners.
            for (FIntPoint P : {FIntPoint(1,1), FIntPoint(2015,1), FIntPoint(1,2015), FIntPoint(2015,2015), FIntPoint(1008,1008), FIntPoint(904,666), FIntPoint(1691,1178)})
            {
                uint16 Actual=0;
                Edit.GetHeightDataFast(P.X,P.Y,P.X,P.Y,&Actual,1);
                Require(Actual==Heights[P.Y*VertexCount+P.X], TEXT("Imported edit-layer height differs from R16"));
                const double ExpectedZ=(double(Heights[P.Y*VertexCount+P.X])-32768)*50/128;
                FHitResult Hit;
                const FVector Start(P.X*200, P.Y*200, 20000), End(P.X*200, P.Y*200, -20000);
                const bool HitGround=World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility);
                Require(HitGround && Cast<ALandscapeProxy>(Hit.GetActor()), TEXT("Landscape collision trace missed"));
                if (HitGround)
                {
                    MaxCollisionErrorCm=FMath::Max(MaxCollisionErrorCm,FMath::Abs(Hit.Location.Z-ExpectedZ));
                    Require(FMath::Abs(Hit.Location.Z-ExpectedZ)<2.0, TEXT("Collision elevation differs by >=2 cm"));
                }
                ++SampleCount;
            }
        }
    }
    TSharedRef<FJsonObject> Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"),Errors.IsEmpty());
    Result->SetBoolField(TEXT("historically_accepted"),false);
    Result->SetNumberField(TEXT("landscape_components"),ComponentCount);
    Result->SetNumberField(TEXT("collision_components"),CollisionCount);
    Result->SetNumberField(TEXT("landscape_actors_including_proxies"),ProxyCount);
    Result->SetNumberField(TEXT("height_and_collision_checks"),SampleCount);
    Result->SetNumberField(TEXT("max_collision_error_cm"),MaxCollisionErrorCm);
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString& Error : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("errors"),ErrorValues);
    FString Json;
    FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    return Json;
}
