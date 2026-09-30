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
#include "LandscapeDataAccess.h"
#include "LandscapeSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/StaticMeshActor.h"
#include "LandscapeLayerInfoObject.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"

namespace CantonTerrain
{
    constexpr int32 VertexCount = 2017;
    bool IsPrototypeWorld(const UWorld* World)
    {
        if (!World || !World->GetWorldPartition()) return false;
        const FString Name = World->GetPackage()->GetName();
        return Name.StartsWith(TEXT("/Game/Canton/Provisional/")) ||
            Name.StartsWith(TEXT("/Game/Canton/DistrictPrototype/"));
    }
    bool ReadHeights(const FString& Path, TArray<uint16>& Heights)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != VertexCount * VertexCount * 2) return false;
        Heights.SetNumUninitialized(VertexCount * VertexCount);
        // Explicit little-endian decoder. The file is already south-first for UE +Y=north.
        for (int32 I = 0; I < Heights.Num(); ++I) Heights[I] = Bytes[2*I] | (uint16(Bytes[2*I+1]) << 8);
        return true;
    }

    struct FWallFoundationSegment
    {
        FVector2D A = FVector2D::ZeroVector;
        FVector2D B = FVector2D::ZeroVector;
    };

    bool ReadWallFoundationProfile(const FString& Path, TArray<FWallFoundationSegment>& Segments,
        double& HalfWidthCm, double& BlendWidthCm, double& MaxAdjustmentCm)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
        HalfWidthCm = Root->GetNumberField(TEXT("corridor_half_width_cm"));
        BlendWidthCm = Root->GetNumberField(TEXT("blend_width_cm"));
        MaxAdjustmentCm = Root->GetNumberField(TEXT("max_adjustment_cm"));
        const TArray<TSharedPtr<FJsonValue>>* JsonSegments = nullptr;
        if (!Root->TryGetArrayField(TEXT("segments"), JsonSegments) || !JsonSegments) return false;
        for (const TSharedPtr<FJsonValue>& Value : *JsonSegments)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid()) continue;
            const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* B = nullptr;
            if (!Object->TryGetArrayField(TEXT("a_cm"), A) || !Object->TryGetArrayField(TEXT("b_cm"), B) ||
                !A || !B || A->Num() != 2 || B->Num() != 2) continue;
            FWallFoundationSegment Segment;
            Segment.A = FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
            Segment.B = FVector2D((*B)[0]->AsNumber(), (*B)[1]->AsNumber());
            if (!Segment.A.Equals(Segment.B, 0.01f)) Segments.Add(Segment);
        }
        return Segments.Num() > 0 && HalfWidthCm > 0.0 && BlendWidthCm > 0.0 && MaxAdjustmentCm > 0.0;
    }

    double HeightCodeToCm(uint16 Code, double ScaleZ)
    {
        return (static_cast<double>(Code) - 32768.0) * ScaleZ / 128.0;
    }

    double SampleHeightCm(const TArray<uint16>& Heights, double Xcm, double Ycm, double ScaleZ)
    {
        const double FX = FMath::Clamp(Xcm / 200.0, 0.0, static_cast<double>(VertexCount - 1));
        const double FY = FMath::Clamp(Ycm / 200.0, 0.0, static_cast<double>(VertexCount - 1));
        const int32 X0 = FMath::Clamp(FMath::FloorToInt(FX), 0, VertexCount - 1);
        const int32 Y0 = FMath::Clamp(FMath::FloorToInt(FY), 0, VertexCount - 1);
        const int32 X1 = FMath::Min(X0 + 1, VertexCount - 1);
        const int32 Y1 = FMath::Min(Y0 + 1, VertexCount - 1);
        const double U = FX - X0;
        const double V = FY - Y0;
        const auto H = [&Heights, ScaleZ](int32 X, int32 Y)
        {
            return HeightCodeToCm(Heights[Y * VertexCount + X], ScaleZ);
        };
        return (1.0 - U) * (1.0 - V) * H(X0, Y0) + U * (1.0 - V) * H(X1, Y0) +
            (1.0 - U) * V * H(X0, Y1) + U * V * H(X1, Y1);
    }

    bool NearestFoundation(const TArray<FWallFoundationSegment>& Segments, const FVector2D& Point,
        double& OutDistanceCm, FVector2D& OutClosestPoint)
    {
        double BestDistanceSq = TNumericLimits<double>::Max();
        bool Found = false;
        for (const FWallFoundationSegment& Segment : Segments)
        {
            const FVector2D Delta = Segment.B - Segment.A;
            const double LengthSq = Delta.SizeSquared();
            const double T = LengthSq > 0.0001 ? FMath::Clamp(FVector2D::DotProduct(Point - Segment.A, Delta) / LengthSq, 0.0, 1.0) : 0.0;
            const FVector2D Closest = Segment.A + Delta * T;
            const double DistanceSq = (Point - Closest).SizeSquared();
            if (DistanceSq < BestDistanceSq)
            {
                BestDistanceSq = DistanceSq;
                OutClosestPoint = Closest;
                Found = true;
            }
        }
        OutDistanceCm = Found ? FMath::Sqrt(BestDistanceSq) : TNumericLimits<double>::Max();
        return Found;
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

UWorld* UCantonTerrainLibrary::CreateDistrictWorld()
{
    const FString Path=TEXT("/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL");
    if (FPackageName::DoesPackageExist(Path)) return nullptr;
    UWorld* World=GEditor->NewMap(true);
    if (!World || !UEditorLoadingAndSavingUtils::SaveMap(World,Path)) return nullptr;
    return World;
}

bool UCantonTerrainLibrary::LoadProvisionalRegion(UWorld* World)
{
    if (!CantonTerrain::IsPrototypeWorld(World)) return false;
    World->GetWorldPartition()->LoadLastLoadedRegions({FBox(FVector(-1000,-1000,-20000),FVector(404200,404200,20000))});
    return true;
}

bool UCantonTerrainLibrary::ImportProvisionalTerrain(UWorld* World, const FString& RawHeightPath)
{
    using namespace CantonTerrain;
    if (!IsPrototypeWorld(World)) return false;
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

bool UCantonTerrainLibrary::AddDistrictEditLayers(UWorld* World)
{
    if (!CantonTerrain::IsPrototypeWorld(World) ||
        !World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/DistrictPrototype/"))) return false;
    ALandscape* Landscape = nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (Landscape) return false;
        Landscape = *It;
    }
    if (!Landscape || !Landscape->GetLayerConst(TEXT("Base_Imported")) ||
        !Landscape->GetLayerConst(TEXT("Base_Imported"))->bLocked) return false;
    for (const FName Name : {FName(TEXT("Urban_Grading")), FName(TEXT("Road_Corridors")), FName(TEXT("Drainage"))})
    {
        if (Landscape->GetLayerConst(Name)) return false;
        if (Landscape->CreateLayer(Name) == INDEX_NONE) return false;
    }
    Landscape->ForceLayersFullUpdate();
    Landscape->MarkPackageDirty();
    return true;
}

FString UCantonTerrainLibrary::ApplyProvisionalWallFoundation(UWorld* World, const FString& ProfilePath)
{
    using namespace CantonTerrain;
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"), false);
    Result->SetStringField(TEXT("profile"), ProfilePath);
    if (!IsPrototypeWorld(World) || !World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/Provisional/")))
    {
        Result->SetStringField(TEXT("error"), TEXT("Not the provisional Canton world"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }

    TArray<FWallFoundationSegment> Segments;
    double HalfWidthCm = 0.0, BlendWidthCm = 0.0, MaxAdjustmentCm = 0.0;
    if (!ReadWallFoundationProfile(ProfilePath, Segments, HalfWidthCm, BlendWidthCm, MaxAdjustmentCm))
    {
        Result->SetStringField(TEXT("error"), TEXT("Wall foundation profile is invalid"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }

    ALandscape* Landscape = nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (Landscape)
        {
            Result->SetStringField(TEXT("error"), TEXT("Multiple Landscapes found"));
            return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
        }
        Landscape = *It;
    }
    if (!Landscape)
    {
        Result->SetStringField(TEXT("error"), TEXT("Landscape missing"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }
    const FLandscapeLayer* BaseLayer = Landscape->GetLayerConst(TEXT("Base_Imported"));
    if (!BaseLayer || !BaseLayer->bLocked)
    {
        Result->SetStringField(TEXT("error"), TEXT("Locked Base_Imported layer missing"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }
    ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
    if (!Info || Info->ComponentSizeQuads <= 0)
    {
        Result->SetStringField(TEXT("error"), TEXT("Landscape info unavailable"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }

    const int32 MaxCoord = VertexCount - 1;
    TArray<uint16> BaseHeights;
    BaseHeights.SetNumUninitialized(VertexCount * VertexCount);
    FLandscapeEditDataInterface BaseEdit(Info, BaseLayer->Guid, false);
    BaseEdit.GetHeightDataFast(0, 0, MaxCoord, MaxCoord, BaseHeights.GetData(), 0);

    int32 LayerIndex = Landscape->GetLayerIndex(FName(TEXT("Wall_Foundation_Adjustment")));
    if (LayerIndex == INDEX_NONE)
    {
        LayerIndex = Landscape->CreateLayer(FName(TEXT("Wall_Foundation_Adjustment")));
    }
    if (LayerIndex == INDEX_NONE)
    {
        Result->SetStringField(TEXT("error"), TEXT("Could not create foundation edit layer"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }
    Landscape->SetLayerBlendMode(LayerIndex, LSBM_AdditiveBlend);
    Landscape->SetLayerAlpha(LayerIndex, 1.0f, true);
    Landscape->SetLayerLocked(LayerIndex, false);
    const FLandscapeLayer* FoundationLayer = Landscape->GetLayerConst(LayerIndex);
    if (!FoundationLayer)
    {
        Result->SetStringField(TEXT("error"), TEXT("Foundation layer lookup failed"));
        return [&Result](){ FString Out; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out)); return Out; }();
    }

    TArray<uint16> AdjustedHeights;
    AdjustedHeights.Init(LandscapeDataAccess::GetTexHeight(0.0f), VertexCount * VertexCount);
    int32 ChangedVertices = 0;
    double MaxAppliedAdjustment = 0.0;
    double MinAppliedAdjustment = 0.0;
    const double ScaleZ = static_cast<double>(Info->DrawScale.Z);
    const double Pi = 3.14159265358979323846;
    for (int32 Y = 0; Y <= MaxCoord; ++Y)
    {
        for (int32 X = 0; X <= MaxCoord; ++X)
        {
            const FVector2D Point(static_cast<double>(X) * 200.0, static_cast<double>(Y) * 200.0);
            double DistanceCm = 0.0;
            FVector2D ClosestPoint = FVector2D::ZeroVector;
            if (!NearestFoundation(Segments, Point, DistanceCm, ClosestPoint)) continue;
            if (DistanceCm > HalfWidthCm + BlendWidthCm) continue;
            const double CenterlineHeight = SampleHeightCm(BaseHeights, ClosestPoint.X, ClosestPoint.Y, ScaleZ);
            const double BaseHeight = HeightCodeToCm(BaseHeights[Y * VertexCount + X], ScaleZ);
            double Weight = 1.0;
            if (DistanceCm > HalfWidthCm)
            {
                const double T = FMath::Clamp((DistanceCm - HalfWidthCm) / BlendWidthCm, 0.0, 1.0);
                Weight = 0.5 * (1.0 + FMath::Cos(Pi * T));
            }
            const double AppliedAdjustment = FMath::Clamp((CenterlineHeight - BaseHeight) * Weight,
                -MaxAdjustmentCm, MaxAdjustmentCm);
            const int32 HeightDelta = FMath::RoundToInt(AppliedAdjustment * 128.0 / ScaleZ);
            if (HeightDelta == 0) continue;
            const int32 NewCode = FMath::Clamp(32768 + HeightDelta, 0, 65535);
            AdjustedHeights[Y * VertexCount + X] = static_cast<uint16>(NewCode);
            ++ChangedVertices;
            MaxAppliedAdjustment = FMath::Max(MaxAppliedAdjustment, AppliedAdjustment);
            MinAppliedAdjustment = FMath::Min(MinAppliedAdjustment, AppliedAdjustment);
        }
    }

    FLandscapeEditDataInterface FoundationEdit(Info, FoundationLayer->Guid, true);
    FoundationEdit.SetHeightData(0, 0, MaxCoord, MaxCoord, AdjustedHeights.GetData(), 0, true,
        nullptr, nullptr, nullptr, false, nullptr, nullptr, true, true, true);
    Landscape->ForceLayersFullUpdate();
    Landscape->RequestLayersContentUpdateForceAll();
    Landscape->MarkPackageDirty();
    World->MarkPackageDirty();

    Result->SetBoolField(TEXT("passed"), true);
    Result->SetStringField(TEXT("layer"), TEXT("Wall_Foundation_Adjustment"));
    Result->SetNumberField(TEXT("segment_count"), Segments.Num());
    Result->SetNumberField(TEXT("changed_vertices"), ChangedVertices);
    Result->SetNumberField(TEXT("corridor_half_width_cm"), HalfWidthCm);
    Result->SetNumberField(TEXT("blend_width_cm"), BlendWidthCm);
    Result->SetNumberField(TEXT("max_adjustment_cm"), MaxAppliedAdjustment);
    Result->SetNumberField(TEXT("min_adjustment_cm"), MinAppliedAdjustment);
    FString Out;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out));
    return Out;
}

FString UCantonTerrainLibrary::ValidateProvisionalWallFoundation(UWorld* World)
{
    TArray<FString> Errors;
    auto Require = [&Errors](bool Good, const TCHAR* Message)
    {
        if (!Good) Errors.Add(Message);
    };
    Require(CantonTerrain::IsPrototypeWorld(World) && World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/Provisional/")),
        TEXT("Not the provisional Canton world"));
    ALandscape* Landscape = nullptr;
    int32 LandscapeCount = 0;
    int32 AdaptiveCount = 0;
    int32 OldCount = 0;
    int32 CollisionMissing = 0;
    double MaxModuleLengthCm = 0.0;
    if (World)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            Landscape = *It;
            ++LandscapeCount;
        }
        Require(LandscapeCount == 1, TEXT("Expected exactly one Landscape"));
        if (Landscape)
        {
            const FLandscapeLayer* Base = Landscape->GetLayerConst(TEXT("Base_Imported"));
            const FLandscapeLayer* Foundation = Landscape->GetLayerConst(TEXT("Wall_Foundation_Adjustment"));
            Require(Base && Base->bLocked, TEXT("Locked Base_Imported layer missing"));
            Require(Foundation != nullptr, TEXT("Wall_Foundation_Adjustment layer missing"));
        }
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const FString Label = It->GetActorLabel();
            if (Label.StartsWith(TEXT("PROVISIONAL_WallAdaptive_")))
            {
                ++AdaptiveCount;
                const FVector Scale = It->GetActorScale3D();
                MaxModuleLengthCm = FMath::Max(MaxModuleLengthCm, FMath::Abs(static_cast<double>(Scale.X)) * 100.0);
                Require(It->GetActorEnableCollision(), TEXT("Adaptive wall collision disabled"));
            }
            else if (Label.StartsWith(TEXT("PROVISIONAL_Wall_")) || Label.StartsWith(TEXT("PROVISIONAL_WallFit_")))
            {
                ++OldCount;
            }
        }
    }
    Require(AdaptiveCount > 0, TEXT("Adaptive wall modules missing"));
    Require(OldCount == 0, TEXT("Coarse wall actors remain"));
    Require(MaxModuleLengthCm <= 250.01, TEXT("Adaptive module exceeds 2.5 m"));
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"), Errors.IsEmpty());
    Result->SetBoolField(TEXT("historically_accepted"), false);
    Result->SetNumberField(TEXT("landscape_count"), LandscapeCount);
    Result->SetNumberField(TEXT("adaptive_module_count"), AdaptiveCount);
    Result->SetNumberField(TEXT("old_coarse_wall_count"), OldCount);
    Result->SetNumberField(TEXT("max_module_length_cm"), MaxModuleLengthCm);
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString& Error : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("errors"), ErrorValues);
    FString Out;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Out));
    return Out;
}

bool UCantonTerrainLibrary::ConfigureDistrictLandscapeMaterial(UWorld* World, UMaterialInterface* Material)
{
    if (!CantonTerrain::IsPrototypeWorld(World) || !Material ||
        !World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/DistrictPrototype/"))) return false;
    ALandscape* Landscape=nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It) Landscape=*It;
    if (!Landscape || Landscape->GetLayerCount()!=4) return false;
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        It->LandscapeMaterial=Material;
        It->MarkPackageDirty();
    }
    const TArray<FName> Names={TEXT("Soil"),TEXT("Earth"),TEXT("Pebble"),TEXT("Grass"),TEXT("Damp"),TEXT("Stone")};
    for (const FName Name : Names)
    {
        if (Landscape->HasTargetLayer(Name)) return false;
        ULandscapeLayerInfoObject* Info=Landscape->CreateLayerInfo(*Name.ToString());
        if (!Info) return false;
        Landscape->AddTargetLayer(Name,FLandscapeTargetLayerSettings(Info));
    }
    Landscape->GetLandscapeInfo()->UpdateLayerInfoMap();
    Landscape->ForceLayersFullUpdate();
    Landscape->MarkPackageDirty();
    return true;
}

FString UCantonTerrainLibrary::ValidateDistrictWorld(UWorld* World, const FString& RawHeightPath)
{
    TArray<FString> Errors;
    auto Require = [&Errors](bool Good, const TCHAR* Message) { if (!Good) Errors.Add(Message); };
    Require(CantonTerrain::IsPrototypeWorld(World) && World->GetPackage()->GetName().StartsWith(TEXT("/Game/Canton/DistrictPrototype/")), TEXT("District WP map missing"));
    TArray<uint16> Heights;
    Require(CantonTerrain::ReadHeights(RawHeightPath, Heights), TEXT("Reference R16 invalid"));
    ALandscape* Landscape = nullptr;
    int32 MainSlabs=0, MixedSlabs=0, Gutters=0, Weeds=0, Damp=0, Gate=0, ParcelPads=0;
    TMap<int32,AActor*> MainRoadByStationY;
    int32 CollisionChecks=0;
    double MaxRoadCenterErrorCm=0;
    double MaxRoadLongEdgeErrorCm=0;
    double MaxRoadJointStepCm=0;
    int32 RoadJointComparisons=0;
    FString FirstRoadHeightSample;
    if (World)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It) Landscape=*It;
        Require(Landscape != nullptr, TEXT("Landscape missing"));
        if (Landscape)
        {
            const FLandscapeLayer* Base=Landscape->GetLayerConst(TEXT("Base_Imported"));
            Require(Base && Base->bLocked, TEXT("Locked imported base missing"));
            for (const TCHAR* Name : {TEXT("Urban_Grading"), TEXT("Road_Corridors"), TEXT("Drainage")})
                Require(Landscape->GetLayerConst(FName(Name)) != nullptr, TEXT("District edit layer missing"));
            Require(Landscape->GetLayerCount()==4, TEXT("Unexpected district edit-layer count"));
            Require(Landscape->LandscapeMaterial && Landscape->LandscapeMaterial->GetPathName().Contains(TEXT("M_Canton_Landscape_PROVISIONAL")), TEXT("District paint material missing"));
            for (const TCHAR* Name : {TEXT("Soil"),TEXT("Earth"),TEXT("Pebble"),TEXT("Grass"),TEXT("Damp"),TEXT("Stone")})
                Require(Landscape->HasTargetLayer(FName(Name)), TEXT("District paint target layer missing"));
        }
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor=*It;
            const FString Label=Actor->GetActorLabel();
            if (Label.StartsWith(TEXT("District_Main_Stone_")))
            {
                ++MainSlabs;
                MainRoadByStationY.Add(FMath::RoundToInt(Actor->GetActorLocation().Y),Actor);
                if (Heights.Num()==CantonTerrain::VertexCount*CantonTerrain::VertexCount)
                {
                    const FVector P=Actor->GetActorLocation();
                    const double FX=FMath::Clamp(P.X/200.0,0.0,2016.0);
                    const double FY=FMath::Clamp(P.Y/200.0,0.0,2016.0);
                    const int32 X=FMath::FloorToInt(FX), Y=FMath::FloorToInt(FY);
                    const int32 X1=FMath::Min(X+1,2016), Y1=FMath::Min(Y+1,2016);
                    const double U=FX-X, V=FY-Y;
                    auto Z=[&Heights](int32 CX,int32 CY){return (double(Heights[CY*2017+CX])-32768)*50/128;};
                    const double Expected=(1-U)*(1-V)*Z(X,Y)+U*(1-V)*Z(X1,Y)+
                        (1-U)*V*Z(X,Y1)+U*V*Z(X1,Y1)+13;
                    const double Error=FMath::Abs(P.Z-Expected);
                    MaxRoadCenterErrorCm=FMath::Max(MaxRoadCenterErrorCm,Error);
                    for (const double Side : {-100.0,100.0})
                    {
                        const double EdgeFY=FMath::Clamp((P.Y+Side)/200.0,0.0,2016.0);
                        const int32 EY=FMath::FloorToInt(EdgeFY), EY1=FMath::Min(EY+1,2016);
                        const double EV=EdgeFY-EY;
                        const double Ground=(1-U)*(1-EV)*Z(X,EY)+U*(1-EV)*Z(X1,EY)+
                            (1-U)*EV*Z(X,EY1)+U*EV*Z(X1,EY1)+13;
                        const double EdgeZ=P.Z+Actor->GetActorRotation().RotateVector(FVector(0,Side,0)).Z;
                        MaxRoadLongEdgeErrorCm=FMath::Max(MaxRoadLongEdgeErrorCm,FMath::Abs(EdgeZ-Ground));
                    }
                    if (FirstRoadHeightSample.IsEmpty())
                        FirstRoadHeightSample=FString::Printf(TEXT("%s actual=%.3f expected=%.3f x=%.3f y=%.3f"),*Label,P.Z,Expected,P.X,P.Y);
                    Require(FMath::Abs(P.Z-Expected)<1.0, TEXT("Stone slab does not follow reference raster"));
                    FHitResult Hit;
                    const bool HitSurface=World->LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,100),ECC_Visibility);
                    Require(HitSurface, TEXT("Stone road collision trace missed"));
                    ++CollisionChecks;
                }
            }
            else if (Label.StartsWith(TEXT("District_MixedLane_"))) ++MixedSlabs;
            else if (Label.StartsWith(TEXT("District_CoveredGutter_"))) ++Gutters;
            else if (Label.StartsWith(TEXT("District_Weed_"))) ++Weeds;
            else if (Label.StartsWith(TEXT("District_LocalDamp_"))) ++Damp;
            else if (Label.StartsWith(TEXT("District_Parcel_"))) ++ParcelPads;
            else if (Label.StartsWith(TEXT("Wenmingmen_PROVISIONAL_"))) ++Gate;
        }
    }
    // Compare the two top-edge corners on every pair of adjacent 2 m road
    // slabs. The older longitudinal-edge check compares each slab to the R16,
    // but cannot detect a visible step across a slab-to-slab joint.
    for (const auto& Pair : MainRoadByStationY)
    {
        AActor* const* Next=MainRoadByStationY.Find(Pair.Key+200);
        if (!Next) continue;
        for (const double LocalX : {-50.0,50.0})
        {
            const FVector ThisTop=Pair.Value->GetActorTransform().TransformPosition(FVector(LocalX,50,50));
            const FVector NextTop=(*Next)->GetActorTransform().TransformPosition(FVector(LocalX,-50,50));
            MaxRoadJointStepCm=FMath::Max(MaxRoadJointStepCm,FMath::Abs(ThisTop.Z-NextTop.Z));
            ++RoadJointComparisons;
        }
    }
    Require(MainSlabs==100, TEXT("Expected 100 main-road slabs"));
    Require(RoadJointComparisons==198, TEXT("Main-road joint sample incomplete"));
    Require(MaxRoadLongEdgeErrorCm<2.0, TEXT("Main-road slab long edges do not follow terrain within 2 cm"));
    Require(MixedSlabs==76, TEXT("Expected 76 mixed-lane slabs"));
    Require(Gutters==48, TEXT("Expected 48 covered gutters"));
    Require(Weeds==38 && Damp==14, TEXT("Expected 38 growth proxies and 14 wet cards"));
    Require(Gate==1, TEXT("Expected one scale-only gate integration asset"));
    Require(ParcelPads==0, TEXT("Unsupported broad parcel pads remain"));
    TSharedRef<FJsonObject> Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"),Errors.IsEmpty());
    Result->SetBoolField(TEXT("historically_accepted"),false);
    Result->SetNumberField(TEXT("main_slabs"),MainSlabs);
    Result->SetNumberField(TEXT("mixed_slabs"),MixedSlabs);
    Result->SetNumberField(TEXT("gutters"),Gutters);
    Result->SetNumberField(TEXT("weed_instances"),Weeds);
    Result->SetNumberField(TEXT("wet_patches"),Damp);
    Result->SetNumberField(TEXT("gate_assets"),Gate);
    Result->SetNumberField(TEXT("broad_parcel_pads"),ParcelPads);
    Result->SetNumberField(TEXT("road_collision_checks"),CollisionChecks);
    Result->SetNumberField(TEXT("max_road_center_error_cm"),MaxRoadCenterErrorCm);
    Result->SetNumberField(TEXT("max_road_long_edge_error_cm"),MaxRoadLongEdgeErrorCm);
    Result->SetNumberField(TEXT("max_road_joint_step_cm"),MaxRoadJointStepCm);
    Result->SetNumberField(TEXT("road_joint_comparisons"),RoadJointComparisons);
    Result->SetStringField(TEXT("first_road_height_sample"),FirstRoadHeightSample);
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString& Error : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("errors"),ErrorValues);
    FString Json;
    FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    return Json;
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


bool UCantonTerrainLibrary::ConfigureDistrictNavigation(UWorld* World)
{
    if (!CantonTerrain::IsPrototypeWorld(World) || !World->GetPackage()->GetName().Contains(TEXT("DistrictPrototype"))) return false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!It->GetActorLabel().StartsWith(TEXT("Wenmingmen_PROVISIONAL_"))) continue;
        It->Modify(); It->SetActorEnableCollision(false);
        TInlineComponentArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
        for (auto* Component : Components)
        {
            Component->Modify();
            Component->SetCanEverAffectNavigation(false);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        FNavigationSystem::UpdateActorData(**It);
        It->MarkPackageDirty();
    }
    int32 Count=0;
    for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
    {
        It->Modify();
        for (int32 R=0; R<int32(ENavigationDataResolution::MAX); ++R)
        {
            It->SetCellSize(ENavigationDataResolution(R),5.f);
            It->SetCellHeight(ENavigationDataResolution(R),1.f);
            It->SetAgentMaxStepHeight(ENavigationDataResolution(R),20.f);
        }
        It->MarkPackageDirty(); ++Count;
    }
    return Count==1;
}
