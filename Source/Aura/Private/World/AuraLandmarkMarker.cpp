// Copyright Druid Mechanics

#include "World/AuraLandmarkMarker.h"
#include "World/AuraLandmarkWorldSubsystem.h"

AAuraLandmarkMarker::AAuraLandmarkMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	LandmarkId = NAME_None;
	DisplayName = FText::FromString(TEXT("Landmark"));
	ApproachTransform = FTransform::Identity;
}

void AAuraLandmarkMarker::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAuraLandmarkWorldSubsystem* Registry = World->GetSubsystem<UAuraLandmarkWorldSubsystem>())
		{
			Registry->RegisterMarker(this);
		}
	}
}

void AAuraLandmarkMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAuraLandmarkWorldSubsystem* Registry = World->GetSubsystem<UAuraLandmarkWorldSubsystem>())
		{
			Registry->UnregisterMarker(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FTransform AAuraLandmarkMarker::GetWorldApproachTransform() const
{
	return ApproachTransform * GetActorTransform();
}

bool AAuraLandmarkMarker::IsGuideConfigured() const
{
	return bEnabled && !LandmarkId.IsNone() && !GetWorldApproachTransform().GetLocation().ContainsNaN()
		&& FMath::IsFinite(ArrivalRadius) && ArrivalRadius > 0.f;
}
