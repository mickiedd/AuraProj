// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GameplayTagContainer.h"
#include "AuraEconomyTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraStockPolicy : uint8
{
	Finite,
	Unlimited,
};

UENUM(BlueprintType)
enum class EAuraEconomyMutationResult : uint8
{
	Success,
	NotAuthority,
	InvalidDefinition,
	InvalidAmount,
	Overflow,
	InsufficientFunds,
	InsufficientItems,
	InventoryFull,
	InvalidState,
};

UENUM(BlueprintType)
enum class EAuraEconomyInitializationState : uint8
{
	NewEphemeralSession,
	Initialized,
	LoadedPersistent,
};

UENUM(BlueprintType)
enum class EAuraCommerceResultCode : uint8
{
	Success,
	InvalidSession,
	InvalidRequest,
	StaleRequest,
	RequestGap,
	InvalidRequester,
	MerchantUnavailable,
	OutOfRange,
	LineOfSightBlocked,
	OfferUnavailable,
	EligibilityDenied,
	SoldOut,
	InsufficientFunds,
	FullInventory,
	InvalidDefinition,
	RollbackFailed,
	PersistenceFailed,
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMerchantOfferPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	FName OfferId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	FText ItemDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	int64 GrantQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	int64 BuyPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	EAuraStockPolicy StockPolicy = EAuraStockPolicy::Finite;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	int64 CurrentStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	bool bAvailable = false;

	UPROPERTY()
	uint32 StockRevision = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMerchantPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	FName PopulationMemberId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	FName MerchantDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	bool bAvailable = false;

	UPROPERTY()
	uint32 StockRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Merchant")
	TArray<FAuraMerchantOfferPresentation> Offers;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraInventorySlot : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Inventory")
	int64 Quantity = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy|Inventory")
	TArray<FAuraInventorySlot> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAuraInventorySlot, FAuraInventoryList>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FAuraInventoryList> : public TStructOpsTypeTraitsBase2<FAuraInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

USTRUCT(BlueprintType)
struct AURA_API FAuraItemDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName ItemId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName Category = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 StackLimit = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FSoftObjectPath UseAction;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraOfferDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName OfferId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName ItemId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 GrantQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 BuyPrice = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") EAuraStockPolicy StockPolicy = EAuraStockPolicy::Finite;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 InitialStock = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName RequiredRoleId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FGameplayTag RequiredInteractionTag;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMerchantDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName MerchantDefinitionId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") TArray<FName> OfferIds;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") TArray<FName> AllowedWorkProfileIds;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FString OpenSchedulePlaceholder;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEconomySettings
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Economy") FName CurrencyId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 StartingBalance = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 MaximumWalletBalance = MAX_int64;
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int64 MaximumItemSlots = 0;
};

struct AURA_API FAuraEconomySnapshot
{
	int32 SchemaVersion = 0;
	int32 Generation = 0;
	FAuraEconomySettings Settings;
	TSet<FName> CurrencyIds;
	TMap<FName, FAuraItemDefinition> Items;
	TMap<FName, FAuraOfferDefinition> Offers;
	TMap<FName, FAuraMerchantDefinition> Merchants;
};
