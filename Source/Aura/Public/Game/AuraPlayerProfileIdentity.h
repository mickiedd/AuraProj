// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraPlayerProfileIdentity.generated.h"

struct FUniqueNetIdRepl;

/** Stable, non-secret identity used only by the authority persistence layer. */
USTRUCT(BlueprintType)
struct AURA_API FAuraPlayerProfileId
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Identity")
	FString ProviderName;

	UPROPERTY(BlueprintReadOnly, Category = "Persistence|Identity")
	FString UniqueId;

	bool IsValid() const;
	FString ToCanonicalString() const;
	FString BuildSlotName(const FString& WorldPersistenceId) const;

	/** Validates explicit components without ever accepting display names or slot names. */
	static bool ValidateComponents(const FString& InProviderName, const FString& InUniqueId,
		const FString& ExpectedProviderName, bool bAllowDevelopmentFixture, FAuraPlayerProfileId& OutId, FString& OutError);

	/** Resolves the provider-owned net identity delivered by PreLogin/InitNewPlayer. */
	static bool FromUniqueNetId(const FUniqueNetIdRepl& NetId, const FString& ExpectedProviderName,
		bool bAllowDevelopmentFixture, FAuraPlayerProfileId& OutId, FString& OutError);
};
