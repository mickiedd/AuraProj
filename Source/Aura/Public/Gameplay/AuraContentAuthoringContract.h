// Copyright Druid Mechanics
#pragma once
#include "CoreMinimal.h"

struct FAuraAuthoredContentRow
{
	FName Id;
	FName Kind;
	TArray<FName> References;
	int32 BudgetCost = 0;
};

/** In-memory candidate registry used to prove fail-closed, atomic Day 57 publication semantics. */
class AURA_API FAuraContentAuthoringContract final
{
public:
	static bool ValidateAndPublish(const TArray<FAuraAuthoredContentRow>& Candidate,
		const TSet<FName>& KnownReferences, int32 MaximumBudget, int32& InOutGeneration,
		TArray<FAuraAuthoredContentRow>& InOutPublished, FString& OutError);
};
