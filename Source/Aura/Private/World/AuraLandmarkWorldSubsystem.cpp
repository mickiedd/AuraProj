// Copyright Druid Mechanics

#include "World/AuraLandmarkWorldSubsystem.h"
#include "World/AuraLandmarkMarker.h"
#include "Aura/AuraLogChannels.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

void UAuraLandmarkWorldSubsystem::RegisterMarker(AAuraLandmarkMarker* Marker)
{
	if (!IsValid(Marker) || Marker->LandmarkId.IsNone())
	{
		UE_LOG(LogAura, Warning, TEXT("[Landmark] Rejected marker with missing ID: %s"), *GetNameSafe(Marker));
		return;
	}

	if (const TObjectPtr<AAuraLandmarkMarker>* Existing = RegisteredMarkers.Find(Marker->LandmarkId))
	{
		if (Existing->Get() != Marker)
		{
			UE_LOG(LogAura, Error, TEXT("[Landmark] Duplicate ID '%s'; keeping '%s', rejecting '%s'"),
				*Marker->LandmarkId.ToString(), *GetNameSafe(Existing->Get()), *GetNameSafe(Marker));
			return;
		}
	}

	RegisteredMarkers.Add(Marker->LandmarkId, Marker);
	bCatalogDirty = true;
	++CatalogRevision;
	UE_LOG(LogAura, Log, TEXT("[Landmark] Registered %s (%s)"), *Marker->LandmarkId.ToString(), *GetNameSafe(Marker));
}

void UAuraLandmarkWorldSubsystem::UnregisterMarker(AAuraLandmarkMarker* Marker)
{
	if (!IsValid(Marker)) return;
	if (TObjectPtr<AAuraLandmarkMarker>* Existing = RegisteredMarkers.Find(Marker->LandmarkId))
	{
		if (Existing->Get() == Marker)
		{
			RegisteredMarkers.Remove(Marker->LandmarkId);
			bCatalogDirty = true;
			++CatalogRevision;
		}
	}
}

void UAuraLandmarkWorldSubsystem::RebuildCatalog() const
{
	if (!bCatalogDirty) return;
	CachedCatalog.Reset();
	for (const TPair<FName, TObjectPtr<AAuraLandmarkMarker>>& Pair : RegisteredMarkers)
	{
		AAuraLandmarkMarker* Marker = Pair.Value.Get();
		if (!IsValid(Marker)) continue;

		FAuraLandmarkDescriptor& Row = CachedCatalog.AddDefaulted_GetRef();
		Row.LandmarkId = Marker->LandmarkId;
		Row.DisplayName = Marker->DisplayName.IsEmpty() ? FText::FromName(Marker->LandmarkId) : Marker->DisplayName;
		Row.LookAtLocation = Marker->GetActorLocation();
		Row.ApproachTransform = Marker->GetWorldApproachTransform();
		Row.ArrivalRadius = Marker->ArrivalRadius;
		Row.SortOrder = Marker->SortOrder;
		Row.Marker = Marker;
		Row.bAvailable = Marker->IsGuideConfigured();
		if (!Row.bAvailable) Row.UnavailableReason = TEXT("Landmark is not configured");
	}

	// The imported showcase level predates authored marker actors and already tags
	// the two landmark meshes. Keep that level usable while artists author explicit
	// entrance markers: the fallback still requires a projected nav point and is
	// never allowed to route to the mesh centre.
	if (UWorld* World = GetWorld())
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			FName Id = NAME_None;
			FText Name;
			int32 Sort = 0;
			if (Actor->ActorHasTag(TEXT("ZhenhaiTower")))
			{
				Id = TEXT("ZhenhaiTower"); Name = FText::FromString(TEXT("Zhenhai Tower")); Sort = 10;
			}
			else if (Actor->ActorHasTag(TEXT("GreatNorthGate")))
			{
				Id = TEXT("GreatNorthGate"); Name = FText::FromString(TEXT("Great North Gate")); Sort = 20;
			}
			if (Id.IsNone() || RegisteredMarkers.Contains(Id) || CachedCatalog.ContainsByPredicate([Id](const FAuraLandmarkDescriptor& Existing)
			{
				return Existing.LandmarkId == Id;
			})) continue;

			const FBox Bounds = Actor->GetComponentsBoundingBox(true);
			FVector Center = Actor->GetActorLocation();
			FVector Extent = Bounds.GetExtent();
			// Imported Nanite meshes can expose a transient, nonsensical component
			// bounds before derived data is ready. Never route using that value.
			if (!FMath::IsFinite(Extent.X) || !FMath::IsFinite(Extent.Y) || !FMath::IsFinite(Extent.Z) || Extent.SizeSquared() > FMath::Square(100000.f))
			{
				Extent = Id == TEXT("GreatNorthGate") ? FVector(1800.f, 900.f, 1200.f) : FVector(1600.f, 1000.f, 1400.f);
			}
			// Keep the actor's authored ground height. Mesh bounds may be centred
			// above the landscape (the gate is a tall facade), which is not a walkable
			// destination even when its X/Y bounds are useful for clearance.
			FVector Approach = FVector(Center.X, Center.Y - Extent.Y - 300.f, Center.Z);
			FNavLocation Projected;
			bool bProjected = Nav && Nav->ProjectPointToNavigation(Approach, Projected, FVector(500.f, 500.f, 500.f));
			if (!bProjected)
			{
				Approach = FVector(Center.X - Extent.X - 300.f, Center.Y, Center.Z);
				bProjected = Nav && Nav->ProjectPointToNavigation(Approach, Projected, FVector(500.f, 500.f, 500.f));
			}

			FAuraLandmarkDescriptor& Row = CachedCatalog.AddDefaulted_GetRef();
			Row.LandmarkId = Id;
			Row.DisplayName = Name;
			Row.LookAtLocation = Center;
			Row.ApproachTransform = FTransform((Center - (bProjected ? Projected.Location : Approach)).Rotation(), bProjected ? Projected.Location : Approach);
			Row.ArrivalRadius = 150.f;
			Row.SortOrder = Sort;
			Row.bAvailable = bProjected;
			if (!bProjected) Row.UnavailableReason = TEXT("No navigable entrance has been authored");
			UE_LOG(LogAura, Log, TEXT("[Landmark] Fallback catalog row %s available=%s approach=%s source=%s"),
				*Id.ToString(), bProjected ? TEXT("true") : TEXT("false"),
				*Row.ApproachTransform.GetLocation().ToCompactString(), *GetNameSafe(Actor));
		}
	}
	CachedCatalog.Sort([](const FAuraLandmarkDescriptor& A, const FAuraLandmarkDescriptor& B)
	{
		return A.SortOrder == B.SortOrder ? A.LandmarkId.LexicalLess(B.LandmarkId) : A.SortOrder < B.SortOrder;
	});
	bCatalogDirty = false;
}

void UAuraLandmarkWorldSubsystem::GetLandmarkCatalog(TArray<FAuraLandmarkDescriptor>& OutCatalog) const
{
	RebuildCatalog();
	OutCatalog = CachedCatalog;
}

bool UAuraLandmarkWorldSubsystem::ResolveLandmark(FName LandmarkId, FAuraLandmarkDescriptor& OutDescriptor) const
{
	RebuildCatalog();
	if (const FAuraLandmarkDescriptor* Found = CachedCatalog.FindByPredicate([LandmarkId](const FAuraLandmarkDescriptor& Row)
	{
		return Row.LandmarkId == LandmarkId;
	}))
	{
		OutDescriptor = *Found;
		return true;
	}
	return false;
}
