// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/AuraEvadeTypes.h"

/** Deterministic server admission policy. It performs no input binding or movement. */
struct AURA_API FAuraEvadePolicy
{
	static constexpr float MaxDistanceUnits = 350.0f;
	static constexpr float DurationSeconds = 0.25f;
	static constexpr float CooldownSeconds = 2.5f;
	static constexpr int32 MaxCharges = 1;

	static EAuraEvadeResult TryAccept(const FAuraEvadeState& CurrentState, const FAuraEvadeRequest& Request,
		const FAuraEvadeOwnerSnapshot& OwnerSnapshot, bool bIsAuthority, double AuthorityNow,
		FAuraEvadeState& OutState, FAuraEvadeAcceptance& OutAcceptance, FString& OutError);

	static EAuraEvadeSweepResult ResolveSweep(const FAuraEvadeState& CurrentState,
		const FAuraEvadeRequestKey& RequestKey, double AuthorityNow, float TravelDistanceUnits, bool bBlocked,
		FAuraEvadeState& OutState, FString& OutError);

	static bool IsNormalizedHorizontalDirection(const FVector& Direction, FVector& OutNormalizedDirection);
};
