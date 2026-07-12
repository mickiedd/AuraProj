// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUNodeRegistry.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"

FBehaviorUNodeRegistry& FBehaviorUNodeRegistry::Get()
{
	static FBehaviorUNodeRegistry Instance;
	return Instance;
}

void FBehaviorUNodeRegistry::Register(const FString& ClassName, FNodeFactory Factory)
{
	FWriteScopeLock WriteLock(Lock);
	Factories.Add(ClassName, MoveTemp(Factory));
}

UBehaviorUBehaviorNode* FBehaviorUNodeRegistry::Create(const FString& ClassName, UObject* Outer) const
{
	const FNodeFactory* Found = nullptr;
	{
		FReadScopeLock ReadLock(Lock);
		Found = Factories.Find(ClassName);
	}
	if (Found)
	{
		return (*Found)(Outer);
	}
	return nullptr;
}