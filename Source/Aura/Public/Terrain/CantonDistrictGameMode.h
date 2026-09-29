#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Character.h"
#include "CantonDistrictGameMode.generated.h"

/** Isolated engineering pawn; no combat/login dependencies or historical claims. */
UCLASS()
class AURA_API ACantonDistrictCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ACantonDistrictCharacter();
    virtual void Tick(float DeltaSeconds) override;
private:
    bool bRainKeyWasDown=false, bDampVisible=true;
};

/** Opt-in runtime evidence harness for the Canton district only. */
UCLASS()
class AURA_API ACantonDistrictGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACantonDistrictGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    struct FRoute { FString Name; FVector Start; FVector End; };
    TArray<FRoute> Routes;
    TArray<TSharedPtr<class FJsonValue>> Results;
    TArray<double> Frames, GPUs;
    TArray<FVector> Waypoints;
    TArray<FString> Failures;
    FString Mode, Output, RawSamples = TEXT("elapsed_seconds,frame_ms,gpu_ms,draw_calls,rss_bytes,streaming_complete,x_cm,y_cm,z_cm\n");
    int32 RouteIndex=-1, PointIndex=0, DrawCallsMax=0, FailedCellSamples=0, StreamingPendingSamples=0;
    double Started=0, RouteStarted=0, LastTick=0, PeakRSS=0, DistanceCm=0;
    FVector Previous;
    bool bFinished=false, bRouteFell=false, bScreenshotRequested=false;
    ACantonDistrictCharacter* TestPawn() const;
    bool FloorAt(const FVector& XY, FVector& Ground) const;
    void StartRoute();
    void Finish();
};

/** Runtime entry point for the wider, provisional walled-city terrain map. */
UCLASS()
class AURA_API ACantonWalledCityGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACantonWalledCityGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    FString Output;
    double Started=0, ScreenshotAt=0;
    bool bScreenshotRequested=false;
    void Finish();
};
