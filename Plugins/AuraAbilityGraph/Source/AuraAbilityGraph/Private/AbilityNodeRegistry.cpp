// Copyright Druid Mechanics

#include "AbilityNodeRegistry.h"
#include "Nodes/AbilityActionNode.h"

FAuraAbilityNodeRegistry& FAuraAbilityNodeRegistry::Get()
{
    static FAuraAbilityNodeRegistry Instance;
    return Instance;
}

void FAuraAbilityNodeRegistry::Register(const FString& ClassName, FNodeFactory Factory)
{
    FWriteScopeLock WriteLock(Lock);
    Factories.Add(ClassName, MoveTemp(Factory));
}

UAuraAbilityActionNode* FAuraAbilityNodeRegistry::Create(const FString& ClassName, UObject* Outer) const
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
