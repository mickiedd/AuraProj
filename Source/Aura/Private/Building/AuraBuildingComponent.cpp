// Copyright Druid Mechanics

#include "Building/AuraBuildingComponent.h"
#include "Building/AuraPlacementPreviewActor.h"
#include "Building/AuraPlacedBuildingActor.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Interaction/PlayerInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool IsOwnerMounted(const AActor* Owner)
{
	if (!Owner) return false;
	if (!Owner->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass())) return false;
	return IPlayerInterface::Execute_IsMounted(const_cast<AActor*>(Owner));
}

FString NormalizeMapPathForConfigCompare(const FString& InMapPath)
{
	if (InMapPath.IsEmpty())
	{
		return InMapPath;
	}

	FString Normalized = InMapPath;
	int32 LastSlashIndex = INDEX_NONE;
	if (!Normalized.FindLastChar(TEXT('/'), LastSlashIndex) || LastSlashIndex + 1 >= Normalized.Len())
	{
		return Normalized;
	}

	const FString PathPrefix = Normalized.Left(LastSlashIndex + 1);
	FString MapName = Normalized.Mid(LastSlashIndex + 1);

	// PIE worlds rename map packages like UEDPIE_0_Demonstration.
	if (MapName.StartsWith(TEXT("UEDPIE_")))
	{
		int32 Index = 7; // strlen("UEDPIE_")
		while (Index < MapName.Len() && FChar::IsDigit(MapName[Index]))
		{
			++Index;
		}

		if (Index < MapName.Len() && MapName[Index] == TEXT('_'))
		{
			MapName = MapName.Mid(Index + 1);
		}
	}

	return PathPrefix + MapName;
}
}

UAuraBuildingComponent::UAuraBuildingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAuraBuildingComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheGroundAltitudeFromLevelConfig();
}

// ---- Tick -----------------------------------------------------------------

void UAuraBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (BuildingState != EBuildingState::Placing || !PreviewActor) return;

	// Only run placement logic on the client that owns this character.
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasLocalNetOwner()) return;
	if (!IsOwnerMounted(Owner))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("Build mode exited: you dismounted."));
		}
		CancelPlacement();
		return;
	}

	const FTransform PlacementTransform = CalculatePlacementTransform();
	PreviewActor->SetActorTransform(PlacementTransform);

	const bool bValid = IsPlacementValid(PlacementTransform) && IsLocationReachable(PlacementTransform.GetLocation());
	if (bValid != bLastValidState)
	{
		bLastValidState = bValid;
		PreviewActor->SetPlacementValid(bValid);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			42, 0.f, bValid ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[Building] %s  |  %s"),
				bValid ? TEXT("VALID") : TEXT("INVALID"),
				*PlacementTransform.GetLocation().ToCompactString()));
	}
}

// ---- Public API -----------------------------------------------------------

void UAuraBuildingComponent::EnterPlacementMode(UStaticMesh* Mesh)
{
	if (!Mesh || !GetWorld()) return;
	if (!IsOwnerMounted(GetOwner()))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Build mode is only available while mounted on the broom."));
		}
		return;
	}

	CancelPlacement();

	PendingMesh = Mesh;
	PendingYaw = 0.f;
	bLastValidState = false;
	BuildingState = EBuildingState::Placing;

	TSubclassOf<AAuraPlacementPreviewActor> SpawnClass = PlacementPreviewClass;
	if (!SpawnClass) SpawnClass = AAuraPlacementPreviewActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PreviewActor = GetWorld()->SpawnActor<AAuraPlacementPreviewActor>(
		SpawnClass,
		GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity,
		SpawnParams);

	if (PreviewActor)
	{
		PreviewActor->SetPreviewMesh(Mesh);
		PreviewActor->SetActorEnableCollision(false);
	}

	PrimaryComponentTick.SetTickFunctionEnable(true);

	UE_LOG(LogTemp, Log, TEXT("[Building] Entered placement mode: %s"), *Mesh->GetName());
}

void UAuraBuildingComponent::CancelPlacement()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	PendingMesh = nullptr;
	PendingYaw = 0.f;
	BuildingState = EBuildingState::Idle;
	PrimaryComponentTick.SetTickFunctionEnable(false);
}

void UAuraBuildingComponent::ConfirmPlacement()
{
	if (BuildingState != EBuildingState::Placing || !PendingMesh) return;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasLocalNetOwner()) return;
	if (!IsOwnerMounted(Owner))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Cannot place: you are no longer mounted."));
		}
		CancelPlacement();
		return;
	}

	const FTransform PlacementTransform = CalculatePlacementTransform();

	if (!IsPlacementValid(PlacementTransform) || !IsLocationReachable(PlacementTransform.GetLocation()))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Cannot place here!"));
		}
		return;
	}

	ServerRequestPlacement(PendingMesh, PlacementTransform);
	CancelPlacement();
}

void UAuraBuildingComponent::RotatePlacement(float DeltaYaw)
{
	PendingYaw += DeltaYaw;
}

// ---- Helpers --------------------------------------------------------------

FTransform UAuraBuildingComponent::CalculatePlacementTransform() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return FTransform::Identity;

	const FVector CharLoc = Owner->GetActorLocation();
	const FVector Forward = Owner->GetActorForwardVector();

	// Line-trace downward from above the target point to find the ground surface.
	const FVector TraceStart = CharLoc + Forward * PlacementDistance + FVector(0.f, 0.f, 500.f);
	const FVector TraceEnd   = TraceStart - FVector(0.f, 0.f, 1200.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	if (PreviewActor) Params.AddIgnoredActor(PreviewActor);

	FVector GroundPos = CharLoc + Forward * PlacementDistance;
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		GroundPos = Hit.ImpactPoint;
	}
	else
	{
		GroundPos.Z = CharLoc.Z;
	}

	if (bHasGroundAltitudeOverride)
	{
		GroundPos.Z = CachedGroundAltitude;
	}

	GroundPos = SnapToGrid(GroundPos);

	const FRotator PlacementRot(0.f, Owner->GetActorRotation().Yaw + PendingYaw, 0.f);
	return FTransform(PlacementRot, GroundPos);
}

void UAuraBuildingComponent::CacheGroundAltitudeFromLevelConfig()
{
	bHasGroundAltitudeOverride = false;
	CachedGroundAltitude = 0.f;

	UWorld* World = GetWorld();
	if (!World || !World->PersistentLevel)
	{
		return;
	}

	const FString CurrentMapPath = World->PersistentLevel->GetOutermost()->GetName();
	if (CurrentMapPath.IsEmpty())
	{
		return;
	}
	const FString NormalizedCurrentMapPath = NormalizeMapPathForConfigCompare(CurrentMapPath);

	const FString LevelConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("LevelConfig.json"));
	FString LevelConfigJson;
	if (!FFileHelper::LoadFileToString(LevelConfigJson, *LevelConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Building] Failed to read LevelConfig: %s"), *LevelConfigPath);
		return;
	}

	TSharedPtr<FJsonObject> LevelConfigRoot;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LevelConfigJson), LevelConfigRoot) || !LevelConfigRoot.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Building] Failed to parse LevelConfig JSON: %s"), *LevelConfigPath);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelsArray = nullptr;
	if (!LevelConfigRoot->TryGetArrayField(TEXT("levels"), LevelsArray) || LevelsArray == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Building] LevelConfig has no levels array: %s"), *LevelConfigPath);
		return;
	}

	for (const TSharedPtr<FJsonValue>& LevelValue : *LevelsArray)
	{
		const TSharedPtr<FJsonObject> LevelObject = LevelValue.IsValid() ? LevelValue->AsObject() : nullptr;
		if (!LevelObject.IsValid())
		{
			continue;
		}

		FString MapPath;
		if (!LevelObject->TryGetStringField(TEXT("mapPath"), MapPath))
		{
			continue;
		}

		const FString NormalizedConfigMapPath = NormalizeMapPathForConfigCompare(MapPath);
		if (!NormalizedConfigMapPath.Equals(NormalizedCurrentMapPath, ESearchCase::IgnoreCase))
		{
			continue;
		}

		double GroundAltitudeValue = 0.0;
		if (LevelObject->TryGetNumberField(TEXT("groundAltitude"), GroundAltitudeValue))
		{
			bHasGroundAltitudeOverride = true;
			CachedGroundAltitude = static_cast<float>(GroundAltitudeValue);
			UE_LOG(LogTemp, Log, TEXT("[Building] Loaded groundAltitude %.2f for map %s"), CachedGroundAltitude, *NormalizedCurrentMapPath);
		}
		break;
	}
}

bool UAuraBuildingComponent::IsPlacementValid(const FTransform& PlacementTransform) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	// Use the preview mesh bounds for the overlap test; fall back to a conservative box.
	FVector BoxExtent(80.f, 80.f, 40.f);
	if (PreviewActor && PreviewActor->MeshComponent)
	{
		BoxExtent = PreviewActor->MeshComponent->Bounds.BoxExtent * 0.85f;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	if (PreviewActor) Params.AddIgnoredActor(PreviewActor);

	const bool bOverlap = World->OverlapAnyTestByChannel(
		PlacementTransform.GetLocation(),
		PlacementTransform.GetRotation(),
		ECC_WorldStatic,
		FCollisionShape::MakeBox(BoxExtent),
		Params);

	return !bOverlap;
}

bool UAuraBuildingComponent::IsLocationReachable(const FVector& Location) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return true; // No nav system — assume reachable.

	FNavLocation Projected;
	// Check that the placement location can be projected onto the NavMesh within a generous tolerance.
	return NavSys->ProjectPointToNavigation(Location, Projected, FVector(300.f, 300.f, 300.f));
}

FVector UAuraBuildingComponent::SnapToGrid(const FVector& Location) const
{
	if (GridSnapSize <= 0.f) return Location;

	return FVector(
		FMath::RoundToFloat(Location.X / GridSnapSize) * GridSnapSize,
		FMath::RoundToFloat(Location.Y / GridSnapSize) * GridSnapSize,
		Location.Z);
}

// ---- Server RPC -----------------------------------------------------------

void UAuraBuildingComponent::ServerRequestPlacement_Implementation(UStaticMesh* Mesh, FTransform Transform)
{
	if (!Mesh || !GetWorld()) return;

	if (bHasGroundAltitudeOverride)
	{
		FVector SpawnLocation = Transform.GetLocation();
		SpawnLocation.Z = CachedGroundAltitude;
		Transform.SetLocation(SpawnLocation);
	}

	// Re-validate on the server before spawning.
	FVector BoxExtent(80.f, 80.f, 40.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	const bool bOverlap = GetWorld()->OverlapAnyTestByChannel(
		Transform.GetLocation(),
		Transform.GetRotation(),
		ECC_WorldStatic,
		FCollisionShape::MakeBox(BoxExtent),
		Params);

	if (bOverlap)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Building] Server rejected placement at %s - overlap"), *Transform.GetLocation().ToString());
		return;
	}

	TSubclassOf<AAuraPlacedBuildingActor> SpawnClass = PlacedActorClass;
	if (!SpawnClass) SpawnClass = AAuraPlacedBuildingActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAuraPlacedBuildingActor* Placed = GetWorld()->SpawnActor<AAuraPlacedBuildingActor>(SpawnClass, Transform, SpawnParams);
	if (Placed)
	{
		Placed->SetPlacedMesh(Mesh);
		UE_LOG(LogTemp, Log, TEXT("[Building] Placed '%s' at %s"), *Mesh->GetName(), *Transform.GetLocation().ToString());
	}
}
