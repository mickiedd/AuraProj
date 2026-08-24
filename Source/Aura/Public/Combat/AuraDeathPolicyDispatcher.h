// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Combat/AuraCombatTypes.h"
#include "UObject/ObjectKey.h"
#include "AuraDeathPolicyDispatcher.generated.h"

class AAuraGameModeBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraAuthoritativeDeath, const FAuraDeathEvent&);

/** GameMode-owned server boundary for exactly-once death policy dispatch. */
UCLASS(Transient)
class AURA_API UAuraDeathPolicyDispatcher : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AAuraGameModeBase* InGameMode);
	bool DispatchDeath(const FAuraDeathEvent& Event);
	int32 GetDispatchedDeathCount() const { return DispatchedDeathCount; }

	FAuraAuthoritativeDeath OnAuthoritativeDeath;

private:
	TWeakObjectPtr<AAuraGameModeBase> GameMode;
	TMap<FObjectKey, int32> HighestDispatchedSequenceByVictim;
	int32 DispatchedDeathCount = 0;
};
