// Copyright Druid Mechanics
#include "Gameplay/AuraContentAuthoringContract.h"

bool FAuraContentAuthoringContract::ValidateAndPublish(const TArray<FAuraAuthoredContentRow>& Candidate,
	const TSet<FName>& KnownReferences, int32 MaximumBudget, int32& InOutGeneration,
	TArray<FAuraAuthoredContentRow>& InOutPublished, FString& OutError)
{
	OutError.Reset();
	if (Candidate.IsEmpty() || MaximumBudget < 0 || InOutGeneration < 0)
	{ OutError = TEXT("ContentCandidateInvalid"); return false; }
	TSet<FName> Seen;
	int64 TotalBudget = 0;
	for (const FAuraAuthoredContentRow& Row : Candidate)
	{
		if (Row.Id.IsNone() || Row.Kind.IsNone() || Seen.Contains(Row.Id) || Row.BudgetCost < 0)
		{ OutError = TEXT("ContentRowInvalid"); return false; }
		Seen.Add(Row.Id); TotalBudget += Row.BudgetCost;
		for (FName Reference : Row.References)
			if (Reference.IsNone() || !KnownReferences.Contains(Reference))
			{ OutError = FString::Printf(TEXT("ContentReferenceInvalid:%s"), *Reference.ToString()); return false; }
	}
	if (TotalBudget > MaximumBudget) { OutError = TEXT("ContentBudgetExceeded"); return false; }
	if (InOutGeneration == MAX_int32) { OutError = TEXT("ContentGenerationOverflow"); return false; }
	TArray<FAuraAuthoredContentRow> Prepared = Candidate;
	InOutPublished = MoveTemp(Prepared); ++InOutGeneration;
	return true;
}
