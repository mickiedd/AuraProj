#include "Terrain/CantonDistrictGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/GameViewportClient.h"
#include "Engine/DirectionalLight.h"
#include "UnrealClient.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "HAL/PlatformMemory.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonSerializer.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Kismet/KismetSystemLibrary.h"

ACantonDistrictCharacter::ACantonDistrictCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(35.f,90.f);
    GetCharacterMovement()->MaxWalkSpeed=450.f;
    GetCharacterMovement()->MaxStepHeight=20.f;
    GetCharacterMovement()->SetWalkableFloorAngle(35.f);
    GetCharacterMovement()->NavAgentProps.AgentRadius=35.f;
    GetCharacterMovement()->NavAgentProps.AgentHeight=180.f;
    auto* Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("WalkingCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0,0,70));
    Camera->bUsePawnControlRotation=true;
    Camera->FieldOfView=65;
}
void ACantonDistrictCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Probe runs drive input themselves, keeping benchmark routes reproducible.
    FString Probe; if(FParse::Value(FCommandLine::Get(),TEXT("CantonProbe="),Probe)) return;
    auto* PC=Cast<APlayerController>(GetController()); if(!PC || !PC->IsLocalController()) return;
    float MouseX=0,MouseY=0;PC->GetInputMouseDelta(MouseX,MouseY);
    AddControllerYawInput(MouseX);AddControllerPitchInput(-MouseY);
    const FRotator Heading(0,PC->GetControlRotation().Yaw,0);
    const float Forward=float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S));
    const float Right=float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A));
    AddMovementInput(Heading.Vector(),Forward);AddMovementInput(FRotationMatrix(Heading).GetUnitAxis(EAxis::Y),Right);
    const bool RainKey=PC->IsInputKeyDown(EKeys::R);
    if(RainKey && !bRainKeyWasDown)
    {
        bDampVisible=!bDampVisible;
        for(TActorIterator<AActor> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("Canton.Damp"))) It->SetActorHiddenInGame(!bDampVisible);
    }
    bRainKeyWasDown=RainKey;
}

ACantonDistrictGameMode::ACantonDistrictGameMode()
{
    DefaultPawnClass=ACantonDistrictCharacter::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=true;
}
ACantonDistrictCharacter* ACantonDistrictGameMode::TestPawn() const
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    return PC?Cast<ACantonDistrictCharacter>(PC->GetPawn()):nullptr;
}
bool ACantonDistrictGameMode::FloorAt(const FVector& XY,FVector& Ground) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CantonFloor),false,TestPawn());
    if (!GetWorld()->LineTraceSingleByChannel(Hit,FVector(XY.X,XY.Y,8000),FVector(XY.X,XY.Y,-5000),ECC_Visibility,Params)) return false;
    Ground=Hit.ImpactPoint; return true;
}
void ACantonDistrictGameMode::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(),TEXT("CantonProbe="),Mode);
    FParse::Value(FCommandLine::Get(),TEXT("CantonOutput="),Output);
    Started=LastTick=FPlatformTime::Seconds();
    if (Mode.IsEmpty()) { if(GEngine) GEngine->AddOnScreenDebugMessage(-1,12,FColor::White,TEXT("Provisional Canton district: WASD move | mouse look | R dry/damp")); SetActorTickEnabled(false); return; }
    if (!GetWorld()->GetMapName().Contains(TEXT("L_Canton_District_PROVISIONAL")) ||
        (Mode!=TEXT("traversal") && Mode!=TEXT("performance")))
    { Failures.Add(TEXT("Wrong map or probe mode")); Finish(); return; }
    if(Output.IsEmpty()) Output=FPaths::ProjectSavedDir()/TEXT("CantonContinuation/Runtime_")+Mode+TEXT(".json");
    Routes={
        {TEXT("principal"),{180000,123500,0},{180000,138000,0}},
        {TEXT("mixed_west"),{174500,132000,0},{180000,132000,0}},
        {TEXT("mixed_east"),{180000,132000,0},{189000,132000,0}},
        {TEXT("intersection"),{180000,130500,0},{184000,132000,0}},
        {TEXT("threshold"),{180000,125000,0},{180000,121750,0}},
        {TEXT("gutter"),{180000,134000,0},{180900,134000,0}},
        {TEXT("courtyard_reference"),{188000,132000,0},{188000,136000,0}}
    };
    for(const TCHAR* Group:{TEXT("ViewDistance"),TEXT("AntiAliasing"),TEXT("Shadow"),TEXT("GlobalIllumination"),TEXT("Reflection"),TEXT("PostProcess"),TEXT("Texture"),TEXT("Effects"),TEXT("Foliage"),TEXT("Shading")})
        UKismetSystemLibrary::ExecuteConsoleCommand(this,FString::Printf(TEXT("sg.%sQuality 2"),Group));
    UKismetSystemLibrary::ExecuteConsoleCommand(this,TEXT("r.VSync 0"));
    UKismetSystemLibrary::ExecuteConsoleCommand(this,TEXT("t.MaxFPS 0"));
    UKismetSystemLibrary::ExecuteConsoleCommand(this,TEXT("r.ScreenPercentage 100"));
    UKismetSystemLibrary::ExecuteConsoleCommand(this,TEXT("t.IdleWhenNotForeground 0"));
}
void ACantonDistrictGameMode::StartRoute()
{
    ++RouteIndex; UE_LOG(LogTemp,Display,TEXT("Canton route index %d"),RouteIndex); PointIndex=0; bRouteFell=false; DistanceCm=0;
    if(RouteIndex>=Routes.Num()) { Finish(); return; }
    auto* Pawn=TestPawn(); FVector Ground;
    if(!Pawn || !FloorAt(Routes[RouteIndex].Start,Ground)) { Failures.Add(TEXT("Missing pawn/start floor")); Finish(); return; }
    Pawn->GetCharacterMovement()->StopMovementImmediately();
    Pawn->SetActorLocation(Ground+FVector(0,0,92),false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Previous=Pawn->GetActorLocation();
    FVector EndGround;
    if(!FloorAt(Routes[RouteIndex].End,EndGround)) { Failures.Add(TEXT("Missing route end floor")); Finish(); return; }
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,Ground+FVector(0,0,20),EndGround+FVector(0,0,20));
    if(!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength()>FVector::Dist2D(Ground,EndGround)*1.5)
    { Failures.Add(TEXT("Invalid/detouring navigation: ")+Routes[RouteIndex].Name); Finish(); return; }
    Waypoints=Path->PathPoints; Waypoints.Add(EndGround);
    RouteStarted=FPlatformTime::Seconds();
}
void ACantonDistrictGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bFinished || Mode.IsEmpty()) return;
    const double Now=FPlatformTime::Seconds(), Elapsed=Now-Started;
    const double FrameMs=(Now-LastTick)*1000; LastTick=Now;
    if(Elapsed<15) return; // Let initial WP cells and navigation settle before physical traversal.
    auto* Pawn=TestPawn();
    if(!Pawn) { Failures.Add(TEXT("Player pawn not spawned")); Finish(); return; }
    if(Mode==TEXT("traversal"))
    {
        if(RouteIndex<0) { StartRoute(); return; }
        const FVector P=Pawn->GetActorLocation();
        DistanceCm+=FVector::Dist2D(P,Previous); Previous=P;
        bRouteFell|=Pawn->GetCharacterMovement()->IsFalling();
        while(PointIndex<Waypoints.Num() && FVector::Dist2D(P,Waypoints[PointIndex])<35) ++PointIndex;
        if(PointIndex>=Waypoints.Num() || Now-RouteStarted>65)
        {
            auto Row=MakeShared<FJsonObject>();
            const double Error=FVector::Dist2D(P,Routes[RouteIndex].End);
            const bool Pass=Error<50 && !bRouteFell;
            Row->SetStringField(TEXT("route"),Routes[RouteIndex].Name);
            Row->SetBoolField(TEXT("passed"),Pass); Row->SetBoolField(TEXT("fell"),bRouteFell);
            Row->SetNumberField(TEXT("endpoint_error_cm"),Error); Row->SetNumberField(TEXT("walked_cm"),DistanceCm);
            Row->SetNumberField(TEXT("seconds"),Now-RouteStarted);
            Results.Add(MakeShared<FJsonValueObject>(Row));
            if(!Pass) Failures.Add(TEXT("Physical traversal failed: ")+Routes[RouteIndex].Name);
            StartRoute(); return;
        }
        const FVector Direction=(Waypoints[PointIndex]-P).GetSafeNormal2D();
        Pawn->AddMovementInput(Direction,1.f);
        if(auto* PC=Cast<APlayerController>(Pawn->GetController())) PC->SetControlRotation(Direction.Rotation());
    }
    else
    {
        // 200 m out-and-back route. Movement stays physical; no sample-time teleport.
        if(RouteIndex<0)
        {
            FVector G;if(!FloorAt(FVector(180000,123500,0),G)){Failures.Add(TEXT("Missing performance route floor"));Finish();return;}
            Pawn->SetActorLocation(G+FVector(0,0,92),false,nullptr,ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->MaxWalkSpeed=20000.f/180.f;
            RouteIndex=0; Previous=Pawn->GetActorLocation();
        }
        const bool Sampling=Elapsed>=60;
        const double Phase=Sampling?Elapsed-60:0;
        const double TargetY=Phase<90?133500:123500;
        FVector Direction=FVector(0,TargetY-Pawn->GetActorLocation().Y,0).GetSafeNormal();
        if(Sampling){Pawn->AddMovementInput(Direction);DistanceCm+=FVector::Dist2D(Pawn->GetActorLocation(),Previous);}
        Previous=Pawn->GetActorLocation();
        if(auto* PC=Cast<APlayerController>(Pawn->GetController())) PC->SetControlRotation(Direction.Rotation());
        if(Elapsed>=120 && !bScreenshotRequested)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output,TEXT("png")),false,false);
            bScreenshotRequested=true;
        }
        if(Sampling && Elapsed<240)
        {
            Frames.Add(FrameMs); if(GGPUFrameTime>0) GPUs.Add(FPlatformTime::ToMilliseconds(GGPUFrameTime));
            PeakRSS=FMath::Max(PeakRSS,double(FPlatformMemory::GetStats().UsedPhysical));
            DrawCallsMax=FMath::Max(DrawCallsMax,GNumDrawCallsRHI[0]);
            if(auto* WP=GetWorld()->GetSubsystem<UWorldPartitionSubsystem>()) StreamingPendingSamples+=!WP->IsAllStreamingCompleted();
            for(auto* Level:GetWorld()->GetStreamingLevels()) if(Level && Level->GetLevelStreamingState()==ELevelStreamingState::FailedToLoad) ++FailedCellSamples;
            bRouteFell|=Pawn->GetCharacterMovement()->IsFalling();
            const bool StreamingComplete=GetWorld()->GetSubsystem<UWorldPartitionSubsystem>() && GetWorld()->GetSubsystem<UWorldPartitionSubsystem>()->IsAllStreamingCompleted();
            const FVector Position=Pawn->GetActorLocation();
            RawSamples+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%d,%.0f,%d,%.3f,%.3f,%.3f\n"),Elapsed,FrameMs,GGPUFrameTime?FPlatformTime::ToMilliseconds(GGPUFrameTime):-1.,GNumDrawCallsRHI[0],double(FPlatformMemory::GetStats().UsedPhysical),int32(StreamingComplete),Position.X,Position.Y,Position.Z);
        }
        if(Elapsed>=240) Finish();
    }
}
void ACantonDistrictGameMode::Finish()
{
    if(bFinished)return;bFinished=true;
    auto R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("mode"),Mode);R->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    R->SetBoolField(TEXT("historically_accepted"),false);
    R->SetStringField(TEXT("gpu_adapter"),GRHIAdapterName);
    auto Settings=MakeShared<FJsonObject>();
    for(const TCHAR* Name:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),TEXT("r.ScreenPercentage"),TEXT("r.DynamicGlobalIlluminationMethod"),TEXT("r.ReflectionMethod")})
        if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(Name)) Settings->SetNumberField(Name,CVar->GetFloat());
    R->SetObjectField(TEXT("render_settings"),Settings);
    R->SetBoolField(TEXT("editor_binary"),WITH_EDITOR!=0);
    R->SetBoolField(TEXT("pawn_traversal_performed"),TestPawn()!=nullptr && RouteIndex>=0);
    R->SetNumberField(TEXT("capsule_radius_cm"),35);R->SetNumberField(TEXT("capsule_half_height_cm"),90);
    R->SetNumberField(TEXT("max_step_cm"),20);R->SetNumberField(TEXT("walkable_floor_degrees"),35);
    R->SetArrayField(TEXT("routes"),Results);
    if(Mode==TEXT("performance"))
    {
        auto Percentile=[](TArray<double> Values,double Q){if(Values.IsEmpty())return -1.;Values.Sort();return Values[FMath::Clamp(FMath::CeilToInt(Q*Values.Num())-1,0,Values.Num()-1)];};
        const double P95=Percentile(Frames,.95),P99=Percentile(Frames,.99);
        FIntPoint Resolution=GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport?GEngine->GameViewport->Viewport->GetSizeXY():FIntPoint::ZeroValue;
        const bool StreamingEnabled=GetWorld()->GetWorldPartition() && GetWorld()->GetWorldPartition()->IsStreamingEnabled();
        R->SetBoolField(TEXT("world_partition_streaming_enabled"),StreamingEnabled);
        R->SetNumberField(TEXT("streaming_level_count"),GetWorld()->GetStreamingLevels().Num());
        R->SetNumberField(TEXT("warmup_seconds"),60);R->SetNumberField(TEXT("capture_seconds"),180);
        R->SetStringField(TEXT("raw_samples"),FPaths::ChangeExtension(Output,TEXT("csv")));
        FFileHelper::SaveStringToFile(RawSamples,*FPaths::ChangeExtension(Output,TEXT("csv")));
        R->SetNumberField(TEXT("width"),Resolution.X);R->SetNumberField(TEXT("height"),Resolution.Y);
        R->SetNumberField(TEXT("p95_ms"),P95);R->SetNumberField(TEXT("p99_ms"),P99);
        R->SetNumberField(TEXT("gpu_p95_ms"),Percentile(GPUs,.95));R->SetNumberField(TEXT("gpu_samples"),GPUs.Num());
        R->SetNumberField(TEXT("frame_samples"),Frames.Num());R->SetNumberField(TEXT("rss_peak_bytes"),PeakRSS);
        R->SetNumberField(TEXT("draw_calls_max"),DrawCallsMax);R->SetNumberField(TEXT("failed_cell_samples"),FailedCellSamples);
        R->SetNumberField(TEXT("streaming_pending_samples"),StreamingPendingSamples);
        R->SetNumberField(TEXT("walked_cm"),DistanceCm);R->SetBoolField(TEXT("fell"),bRouteFell);
        int32 Instances=0,Meshes=0;for(TActorIterator<AActor> It(GetWorld());It;++It){TInlineComponentArray<UStaticMeshComponent*> Cs;It->GetComponents(Cs);for(auto* C:Cs){++Meshes;if(auto* ISM=Cast<UInstancedStaticMeshComponent>(C))Instances+=ISM->GetInstanceCount();}}
        R->SetNumberField(TEXT("loaded_mesh_components"),Meshes);R->SetNumberField(TEXT("loaded_instanced_instances"),Instances);
        if(WITH_EDITOR || !StreamingEnabled || GetWorld()->GetStreamingLevels().IsEmpty() || Frames.Num()<1000 || GPUs.IsEmpty() || Resolution!=FIntPoint(1920,1080) || P95>33.3 || P99>50 || PeakRSS>12.*1024*1024*1024 || FailedCellSamples || bRouteFell || DistanceCm<19500)
            Failures.Add(TEXT("Runtime acceptance configuration, route or numeric budget failed"));
    }
    TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));
    R->SetArrayField(TEXT("errors"),Errors);R->SetBoolField(TEXT("passed"),Failures.IsEmpty());
    FString Json;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
    const bool Written=FFileHelper::SaveStringToFile(Json,*Output);
    UE_LOG(LogTemp,Display,TEXT("Canton result written=%d path=%s result=%s"),Written,*Output,*Json);
    FPlatformMisc::RequestExit(false);
}

ACantonWalledCityGameMode::ACantonWalledCityGameMode()
{
    DefaultPawnClass=ACantonDistrictCharacter::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
}
void ACantonWalledCityGameMode::BeginPlay()
{
    Super::BeginPlay();
    Started=FPlatformTime::Seconds();
    FParse::Value(FCommandLine::Get(),TEXT("CantonWalledOutput="),Output);
    if(Output.IsEmpty())
    {
        if(GEngine) GEngine->AddOnScreenDebugMessage(-1,12,FColor::White,
            TEXT("Provisional Canton walled-city terrain: WASD move | mouse look"));
        SetActorTickEnabled(false);
    }
}
void ACantonWalledCityGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(Output.IsEmpty()) return;
    const double Elapsed=FPlatformTime::Seconds()-Started;
    auto* WP=GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
    const bool bReady=WP && WP->IsAllStreamingCompleted();
    if(!bScreenshotRequested && ((Elapsed>=12 && bReady) || Elapsed>=35))
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
        FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output,TEXT("png")),false,false);
        ScreenshotAt=FPlatformTime::Seconds();
        bScreenshotRequested=true;
    }
    if(bScreenshotRequested && FPlatformTime::Seconds()-ScreenshotAt>=3) Finish();
}
void ACantonWalledCityGameMode::Finish()
{
    auto R=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Errors;
    auto Error=[&Errors](const TCHAR* Message){Errors.Add(MakeShared<FJsonValueString>(Message));};
    auto* PC=GetWorld()->GetFirstPlayerController();
    auto* Pawn=PC?Cast<ACantonDistrictCharacter>(PC->GetPawn()):nullptr;
    FHitResult Hit;
    bool bFloor=false;
    if(Pawn)
    {
        const FVector Position=Pawn->GetActorLocation();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CantonWalledFloor),false,Pawn);
        bFloor=GetWorld()->LineTraceSingleByChannel(Hit,Position+FVector(0,0,100),
            Position-FVector(0,0,3000),ECC_Visibility,Params) &&
            Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Landscape"));
        R->SetNumberField(TEXT("pawn_x_cm"),Position.X);
        R->SetNumberField(TEXT("pawn_y_cm"),Position.Y);
        R->SetNumberField(TEXT("pawn_z_cm"),Position.Z);
    }
    int32 Lights=0;
    for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It) ++Lights;
    auto* WP=GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
    const bool bStreaming=GetWorld()->GetWorldPartition() && GetWorld()->GetWorldPartition()->IsStreamingEnabled();
    const bool bComplete=WP && WP->IsAllStreamingCompleted();
    int32 FailedCells=0;
    for(auto* Level:GetWorld()->GetStreamingLevels())
        if(Level && Level->GetLevelStreamingState()==ELevelStreamingState::FailedToLoad) ++FailedCells;
    const FString Screenshot=FPaths::ChangeExtension(Output,TEXT("png"));
    const bool bScreenshot=IFileManager::Get().FileExists(*Screenshot);
    R->SetStringField(TEXT("map"),GetWorld()->GetMapName());
    R->SetStringField(TEXT("game_mode"),GetClass()->GetName());
    R->SetBoolField(TEXT("editor_binary"),WITH_EDITOR!=0);
    R->SetBoolField(TEXT("historically_accepted"),false);
    R->SetBoolField(TEXT("pawn_spawned"),Pawn!=nullptr);
    R->SetBoolField(TEXT("landscape_floor_hit"),bFloor);
    R->SetNumberField(TEXT("directional_lights"),Lights);
    R->SetBoolField(TEXT("world_partition_streaming_enabled"),bStreaming);
    R->SetBoolField(TEXT("streaming_complete"),bComplete);
    R->SetNumberField(TEXT("streaming_level_count"),GetWorld()->GetStreamingLevels().Num());
    R->SetNumberField(TEXT("failed_cells"),FailedCells);
    R->SetStringField(TEXT("screenshot"),Screenshot);
    R->SetBoolField(TEXT("screenshot_written"),bScreenshot);
    if(!GetWorld()->GetMapName().Contains(TEXT("L_Canton_WalledCity_PROVISIONAL"))) Error(TEXT("Wrong map"));
    if(!Pawn) Error(TEXT("Player pawn missing"));
    if(!bFloor) Error(TEXT("Landscape floor missing below pawn"));
    if(Lights<1) Error(TEXT("No directional light loaded"));
    if(!bStreaming || !bComplete || GetWorld()->GetStreamingLevels().IsEmpty() || FailedCells)
        Error(TEXT("World Partition streaming not healthy"));
    if(!bScreenshot) Error(TEXT("Screenshot missing"));
    R->SetArrayField(TEXT("errors"),Errors);
    R->SetBoolField(TEXT("passed"),Errors.IsEmpty());
    FString Json;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json,*Output);
    UE_LOG(LogTemp,Display,TEXT("Canton walled visibility report: %s"),*Json);
    FPlatformMisc::RequestExit(false);
}
