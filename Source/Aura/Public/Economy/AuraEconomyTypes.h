// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AuraEconomyTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraStockPolicy : uint8
{
	Finite,
	Unlimited,
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
