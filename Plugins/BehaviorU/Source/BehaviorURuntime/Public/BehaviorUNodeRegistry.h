// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"

class UBehaviorUBehaviorNode;

/**
 * Extensible node-class registry.
 *
 * The XML loader (CreateNodeByClassName in BehaviorUBehaviorTree.cpp) consults this
 * registry before falling back to its built-in if/else chain. External modules
 * register factories for their custom <node class="..."> types at module
 * StartupModule; the loader then picks them up automatically.
 *
 * Backward compatible: an unregistered class name yields nullptr here and the
 * loader proceeds to its built-in chain, preserving existing behavior.
 *
 * Registration MUST happen on the game thread before any behavior tree is loaded
 * (module startup is safe). Lookup is read-only at load time (game thread), but the
 * FRWLock makes concurrent read safe if a future caller loads off-thread.
 */
class BEHAVIORURUNTIME_API FBehaviorUNodeRegistry
{
public:
	/** Factory: create a node definition instance. Outer is the tree asset. */
	using FNodeFactory = TFunction<UBehaviorUBehaviorNode*(UObject* Outer)>;

	/** Access the process-wide registry. */
	static FBehaviorUNodeRegistry& Get();

	/** Register (or replace) a factory for a class name. Call at module startup. */
	void Register(const FString& ClassName, FNodeFactory Factory);

	/**
	 * Create a node instance for the given class name.
	 * @return The new node, or nullptr if no factory is registered for ClassName.
	 *         The caller owns the returned object (NewObject-allocated with Outer).
	 */
	UBehaviorUBehaviorNode* Create(const FString& ClassName, UObject* Outer) const;

private:
	FBehaviorUNodeRegistry() = default;

	TMap<FString, FNodeFactory> Factories;
	mutable FRWLock Lock;
};