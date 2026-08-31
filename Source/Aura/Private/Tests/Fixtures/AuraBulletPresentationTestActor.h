#pragma once

#include "Actor/AuraBullet.h"
#include "AuraBulletPresentationTestActor.generated.h"

/** Native fixture exposes impact delivery without a network driver or rendered viewport. */
UCLASS(Transient, NotBlueprintable)
class AAuraBulletPresentationTestActor : public AAuraBullet
{
	GENERATED_BODY()
public:
	void UseClientRole() { SetRole(ROLE_SimulatedProxy); }
	void DeliverOverlap() { OnHit(); }
	void DeliverServerImpact(const FVector& Location, const FVector& Normal, bool bSurface)
	{
		MulticastPlayImpactEffects_Implementation(Location, Normal, bSurface);
	}
};
