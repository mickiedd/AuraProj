// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraBattleZoneTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraBattlePhase : uint8
{
	Peace,
	Alert,
	Conflict,
	Cleanup,
};

USTRUCT(BlueprintType)
struct AURA_API FAuraBattleZoneDefinition
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	FName ZoneId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	FName MapId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	int32 Priority = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	FVector Extent = FVector(1000.f, 1000.f, 1000.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone")
	bool bSafeZone = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone|Policy")
	bool bAllowPvP = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone|Policy")
	bool bAllowPlayerToCivilian = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone|Policy")
	bool bAllowEnemyToCivilian = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle Zone|Policy")
	bool bTargetProtected = true;

	bool Contains(const FVector& Location) const
	{
		const FVector Delta = Location - Center;
		return FMath::Abs(Delta.X) <= Extent.X && FMath::Abs(Delta.Y) <= Extent.Y && FMath::Abs(Delta.Z) <= Extent.Z;
	}
};
