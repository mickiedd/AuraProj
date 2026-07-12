// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestRunContext.h"

void FAutoTestRunContext::AddAssertion(FString Message, bool bPassed)
{
	FAutoTestAssertion A;
	A.Message = MoveTemp(Message);
	A.bPassed = bPassed;

	FWriteScopeLock WriteLock(Lock);
	Assertions.Add(MoveTemp(A));
}

bool FAutoTestRunContext::SetTerminal(EAutoTestStatus NewStatus)
{
	// Only leave Running; once terminal, status is sticky.
	EAutoTestStatus Expected = EAutoTestStatus::Running;
	return Status.compare_exchange_strong(Expected, NewStatus, std::memory_order_relaxed);
}

TArray<FAutoTestAssertion> FAutoTestRunContext::GetAssertions() const
{
	FReadScopeLock ReadLock(Lock);
	return Assertions;
}

TArray<TWeakObjectPtr<AActor>> FAutoTestRunContext::GetSpawnedActors() const
{
	FReadScopeLock ReadLock(Lock);
	return SpawnedActors;
}

void FAutoTestRunContext::AddSpawnedActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	FWriteScopeLock WriteLock(Lock);
	SpawnedActors.Add(Actor);
}