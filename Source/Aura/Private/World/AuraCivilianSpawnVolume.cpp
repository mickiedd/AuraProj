// Copyright Druid Mechanics

#include "World/AuraCivilianSpawnVolume.h"

#include "Character/AuraCivilian.h"
#include "Aura/AuraLogChannels.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Game/AuraGameModeBase.h"
#include "NavigationSystem.h"
#include "World/AuraPopulationManager.h"

AAuraCivilianSpawnVolume::AAuraCivilianSpawnVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);
	Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Volume->SetGenerateOverlapEvents(false);
	Volume->SetBoxExtent(CandidateExtents);
}

void AAuraCivilianSpawnVolume::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
	{
		if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable())
		{
			Manager->RegisterSpawnVolume(this);
		}
	}
}

void AAuraCivilianSpawnVolume::SetSpawnVolumeIdForRuntime(FName InId)
{
	if (!HasAuthority() || InId.IsNone()) return;
	SpawnVolumeId = InId;
	if (HasActorBegunPlay())
	{
		if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
		{
			if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable()) Manager->RegisterSpawnVolume(this);
		}
	}
}

void AAuraCivilianSpawnVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
		{
			if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable()) Manager->UnregisterSpawnVolume(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool AAuraCivilianSpawnVolume::GetCandidateTransform(int32 AttemptIndex, FTransform& OutTransform) const
{
	if (!IsValid(Volume) || SpawnVolumeId.IsNone() || !GetWorld())
	{
		UE_LOG(LogAura, Warning, TEXT("[Population][Volume] Candidate rejected before query volume=%s id=%s attempt=%d."),
			*GetNameSafe(this), *SpawnVolumeId.ToString(), AttemptIndex);
		return false;
	}

	const FVector Extent = Volume->GetScaledBoxExtent();
	const int32 GridIndex = FMath::Max(0, AttemptIndex);
	const float XAlpha = (static_cast<float>((GridIndex * 37) % 101) / 100.f) * 2.f - 1.f;
	const float YAlpha = (static_cast<float>((GridIndex * 61 + 17) % 101) / 100.f) * 2.f - 1.f;
	const FVector LocalOffset(XAlpha * Extent.X, YAlpha * Extent.Y, Extent.Z);
	FVector WorldLocation = GetActorTransform().TransformPosition(LocalOffset);
	FNavLocation ProjectedLocation;
	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		!NavigationSystem || !NavigationSystem->ProjectPointToNavigation(WorldLocation, ProjectedLocation, Extent))
	{
		UE_LOG(LogAura, Warning, TEXT("[Population][Volume] Candidate rejected: no navigation volume=%s attempt=%d query=%s extent=%s."),
			*GetNameSafe(this), AttemptIndex, *WorldLocation.ToCompactString(), *Extent.ToCompactString());
		return false;
	}

	const AAuraCivilian* CivilianCDO = AAuraCivilian::StaticClass()->GetDefaultObject<AAuraCivilian>();
	const UCapsuleComponent* CivilianCapsule = CivilianCDO ? CivilianCDO->GetCapsuleComponent() : nullptr;
	const float CapsuleRadius = CivilianCapsule ? CivilianCapsule->GetScaledCapsuleRadius() : 34.f;
	const float CapsuleHalfHeight = CivilianCapsule ? CivilianCapsule->GetScaledCapsuleHalfHeight() : 88.f;
	// ProjectPointToNavigation returns the walkable surface. A Character actor's
	// transform is the capsule center, so passing the surface Z directly lets
	// AdjustIfPossibleButAlwaysSpawn lift the actor to escape the floor, which is
	// the source of the visible floating civilians.
	WorldLocation = ProjectedLocation.Location + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AuraCivilianSpawnVolume), false);
	QueryParams.AddIgnoredActor(this);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	if (GetWorld()->OverlapAnyTestByObjectType(
		WorldLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams))
	{
		UE_LOG(LogAura, Warning, TEXT("[Population][Volume] Candidate rejected: blocked volume=%s attempt=%d nav=%s."),
			*GetNameSafe(this), AttemptIndex, *ProjectedLocation.Location.ToCompactString());
		return false;
	}

	OutTransform = FTransform(GetActorRotation(), WorldLocation, FVector::OneVector);
	return true;
}
