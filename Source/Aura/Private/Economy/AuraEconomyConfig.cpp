// Copyright Druid Mechanics

#include "Economy/AuraEconomyConfig.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "World/AuraPopulationTypes.h"

namespace AuraEconomyConfigPrivate
{
	bool TryReadStrictString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutText)
	{
		const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
		return Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(OutText);
	}

	bool LoadRoot(const TCHAR* FileName, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), FileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError += FString::Printf(TEXT("Cannot read %s.\n"), *Path); return false; }
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			OutError += FString::Printf(TEXT("Invalid JSON in %s.\n"), *Path); return false;
		}
		return true;
	}

	bool ReadSchema(const TSharedPtr<FJsonObject>& Root, const TCHAR* FileName, int32& OutSchema, FString& OutError)
	{
		double Value = 0.0;
		if (!Root->TryGetNumberField(TEXT("schemaVersion"), Value) || Value != FMath::FloorToDouble(Value) || Value != 1.0)
		{
			OutError += FString::Printf(TEXT("%s.schemaVersion must be supported integer 1.\n"), FileName); return false;
		}
		OutSchema = static_cast<int32>(Value);
		return true;
	}

	bool ReadId(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FName& OutId, FString& OutError, const FString& Path)
	{
		FString Text;
		if (!TryReadStrictString(Object, Field, Text) || (OutId = FAuraEconomyConfigLoader::NormalizeId(Text)).IsNone())
		{
			OutError += FString::Printf(TEXT("%s.%s must be a non-empty ID.\n"), *Path, Field); return false;
		}
		return true;
	}

	bool ReadOptionalId(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FName& OutId, FString& OutError, const FString& Path)
	{
		if (!Object->HasField(Field)) return true;
		return ReadId(Object, Field, OutId, OutError, Path);
	}

	bool ReadInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool bPositive, int64& OutValue, FString& OutError, const FString& Path)
	{
		FString Text;
		if (!TryReadStrictString(Object, Field, Text))
		{
			OutError += FString::Printf(TEXT("%s.%s must be a canonical base-10 string.\n"), *Path, Field); return false;
		}
		FString ParseError;
		if (!FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(Text, bPositive, OutValue, ParseError))
		{
			OutError += FString::Printf(TEXT("%s.%s: %s\n"), *Path, Field, *ParseError); return false;
		}
		return true;
	}
}

FName FAuraEconomyConfigLoader::NormalizeId(const FString& RawId)
{
	FString Value = RawId;
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty() || Value != RawId) return NAME_None;
	return FName(*Value.ToLower());
}

bool FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(const FString& Text, bool bRequirePositive, int64& OutValue, FString& OutError)
{
	OutValue = 0;
	if (Text.IsEmpty()) { OutError = TEXT("value is empty"); return false; }
	if (Text.Len() > 1 && Text[0] == TCHAR('0')) { OutError = TEXT("leading zero is non-canonical"); return false; }
	uint64 Accumulator = 0;
	for (const TCHAR Char : Text)
	{
		if (Char < TCHAR('0') || Char > TCHAR('9')) { OutError = TEXT("only unsigned decimal digits are accepted"); return false; }
		const uint64 Digit = static_cast<uint64>(Char - TCHAR('0'));
		if (Accumulator > (static_cast<uint64>(MAX_int64) - Digit) / 10u) { OutError = TEXT("int64 overflow"); return false; }
		Accumulator = Accumulator * 10u + Digit;
	}
	OutValue = static_cast<int64>(Accumulator);
	if (bRequirePositive && OutValue == 0) { OutError = TEXT("value must be greater than zero"); return false; }
	return true;
}

bool FAuraEconomyConfigLoader::LoadFromProjectFiles(const TArray<FAuraPopulationSpawnRow>& PopulationRows,
	FAuraEconomySnapshot& OutSnapshot, FString& OutError)
{
	using namespace AuraEconomyConfigPrivate;
	OutSnapshot = FAuraEconomySnapshot();
	OutError.Empty();
	TSharedPtr<FJsonObject> ItemRoot, MerchantRoot, EconomyRoot;
	if (!LoadRoot(TEXT("ItemDefinitions.json"), ItemRoot, OutError)
		|| !LoadRoot(TEXT("MerchantDefinitions.json"), MerchantRoot, OutError)
		|| !LoadRoot(TEXT("EconomyConfig.json"), EconomyRoot, OutError)) return false;
	int32 ItemSchema = 0, MerchantSchema = 0, EconomySchema = 0;
	ReadSchema(ItemRoot, TEXT("ItemDefinitions"), ItemSchema, OutError);
	ReadSchema(MerchantRoot, TEXT("MerchantDefinitions"), MerchantSchema, OutError);
	ReadSchema(EconomyRoot, TEXT("EconomyConfig"), EconomySchema, OutError);
	if (ItemSchema != MerchantSchema || ItemSchema != EconomySchema)
	{
		OutError += TEXT("Economy schema versions are incompatible.\n");
	}
	OutSnapshot.SchemaVersion = EconomySchema;

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!ItemRoot->TryGetArrayField(TEXT("items"), Items)) OutError += TEXT("ItemDefinitions.items must be an array.\n");
	else for (int32 Index = 0; Index < Items->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = (*Items)[Index];
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object) { OutError += FString::Printf(TEXT("items[%d] must be an object.\n"), Index); continue; }
		FAuraItemDefinition Item;
		const FString Path = FString::Printf(TEXT("items[%d]"), Index);
		FString Display, Category, UseAction;
		ReadId(Object, TEXT("itemId"), Item.ItemId, OutError, Path);
		if (!TryReadStrictString(Object, TEXT("displayName"), Display) || Display.IsEmpty()) OutError += Path + TEXT(".displayName is required.\n");
		if (!TryReadStrictString(Object, TEXT("category"), Category) || (Item.Category = NormalizeId(Category)).IsNone()) OutError += Path + TEXT(".category is required.\n");
		Item.DisplayName = FText::FromString(Display);
		ReadInt64(Object, TEXT("stackLimit"), true, Item.StackLimit, OutError, Path);
		if (Object->HasField(TEXT("useAction")) && !TryReadStrictString(Object, TEXT("useAction"), UseAction))
		{
			OutError += Path + TEXT(".useAction must be a string.\n");
		}
		else if (!UseAction.IsEmpty())
		{
			Item.UseAction = FSoftObjectPath(UseAction);
			if (!Item.UseAction.IsValid() || !Item.UseAction.TryLoad()) OutError += Path + TEXT(".useAction must resolve to a loaded asset.\n");
		}
		if (!Item.ItemId.IsNone() && OutSnapshot.Items.Contains(Item.ItemId)) OutError += Path + TEXT(" duplicates itemId.\n");
		else if (!Item.ItemId.IsNone()) OutSnapshot.Items.Add(Item.ItemId, Item);
	}

	const TArray<TSharedPtr<FJsonValue>>* Offers = nullptr;
	if (!MerchantRoot->TryGetArrayField(TEXT("offers"), Offers)) OutError += TEXT("MerchantDefinitions.offers must be an array.\n");
	else for (int32 Index = 0; Index < Offers->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = (*Offers)[Index];
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object) { OutError += FString::Printf(TEXT("offers[%d] must be an object.\n"), Index); continue; }
		FAuraOfferDefinition Offer;
		const FString Path = FString::Printf(TEXT("offers[%d]"), Index);
		ReadId(Object, TEXT("offerId"), Offer.OfferId, OutError, Path);
		FString ItemId, StockPolicy, RequiredRole, RequiredTag;
		if (!TryReadStrictString(Object, TEXT("itemId"), ItemId) || (Offer.ItemId = NormalizeId(ItemId)).IsNone()) OutError += Path + TEXT(".itemId is required.\n");
		ReadInt64(Object, TEXT("grantQuantity"), true, Offer.GrantQuantity, OutError, Path);
		ReadInt64(Object, TEXT("buyPrice"), false, Offer.BuyPrice, OutError, Path);
		if (!TryReadStrictString(Object, TEXT("stockPolicy"), StockPolicy)) OutError += Path + TEXT(".stockPolicy is required.\n");
		else if (StockPolicy.Equals(TEXT("finite"), ESearchCase::IgnoreCase))
		{
			Offer.StockPolicy = EAuraStockPolicy::Finite;
			ReadInt64(Object, TEXT("initialStock"), false, Offer.InitialStock, OutError, Path);
		}
		else if (StockPolicy.Equals(TEXT("unlimited"), ESearchCase::IgnoreCase)) Offer.StockPolicy = EAuraStockPolicy::Unlimited;
		else OutError += Path + TEXT(".stockPolicy must be finite or unlimited.\n");
		ReadOptionalId(Object, TEXT("requiredRoleId"), Offer.RequiredRoleId, OutError, Path);
		if (Object->HasField(TEXT("requiredInteractionTag")) && !TryReadStrictString(Object, TEXT("requiredInteractionTag"), RequiredTag))
		{
			OutError += Path + TEXT(".requiredInteractionTag must be a string.\n");
		}
		else if (Object->HasField(TEXT("requiredInteractionTag")) && RequiredTag.IsEmpty())
		{
			OutError += Path + TEXT(".requiredInteractionTag must be a non-empty gameplay tag.\n");
		}
		else if (!RequiredTag.IsEmpty())
		{
			Offer.RequiredInteractionTag = FGameplayTag::RequestGameplayTag(FName(*RequiredTag), false);
			if (!Offer.RequiredInteractionTag.IsValid()) OutError += Path + TEXT(".requiredInteractionTag is unknown.\n");
		}
		if (!Offer.OfferId.IsNone() && OutSnapshot.Offers.Contains(Offer.OfferId)) OutError += Path + TEXT(" duplicates offerId.\n");
		else if (!Offer.OfferId.IsNone()) OutSnapshot.Offers.Add(Offer.OfferId, Offer);
	}

	const TArray<TSharedPtr<FJsonValue>>* Merchants = nullptr;
	if (!MerchantRoot->TryGetArrayField(TEXT("merchants"), Merchants)) OutError += TEXT("MerchantDefinitions.merchants must be an array.\n");
	else for (int32 Index = 0; Index < Merchants->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& MerchantValue = (*Merchants)[Index];
		const TSharedPtr<FJsonObject> Object = MerchantValue.IsValid() ? MerchantValue->AsObject() : nullptr;
		if (!Object) { OutError += FString::Printf(TEXT("merchants[%d] must be an object.\n"), Index); continue; }
		FAuraMerchantDefinition Merchant;
		const FString Path = FString::Printf(TEXT("merchants[%d]"), Index);
		ReadId(Object, TEXT("merchantDefinitionId"), Merchant.MerchantDefinitionId, OutError, Path);
		const TArray<TSharedPtr<FJsonValue>>* OfferIds = nullptr;
		if (!Object->TryGetArrayField(TEXT("offerIds"), OfferIds) || OfferIds->IsEmpty()) OutError += Path + TEXT(".offerIds must be non-empty.\n");
		else
		{
			TSet<FName> Seen;
			for (const TSharedPtr<FJsonValue>& OfferValue : *OfferIds)
			{
				FString RawId;
				if (!OfferValue.IsValid() || OfferValue->Type != EJson::String || !OfferValue->TryGetString(RawId))
				{
					OutError += Path + TEXT(".offerIds entries must be strings.\n");
					continue;
				}
				const FName Id = NormalizeId(RawId);
				if (Id.IsNone() || Seen.Contains(Id)) OutError += Path + TEXT(".offerIds contains empty or duplicate ID.\n");
				else { Seen.Add(Id); Merchant.OfferIds.Add(Id); }
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Profiles = nullptr;
		if (Object->HasField(TEXT("allowedWorkProfileIds")) && !Object->TryGetArrayField(TEXT("allowedWorkProfileIds"), Profiles))
		{
			OutError += Path + TEXT(".allowedWorkProfileIds must be an array.\n");
		}
		else if (Profiles)
		{
			for (const TSharedPtr<FJsonValue>& ProfileValue : *Profiles)
			{
				FString RawProfile;
				const FName Profile = ProfileValue.IsValid() && ProfileValue->Type == EJson::String && ProfileValue->TryGetString(RawProfile) ? NormalizeId(RawProfile) : NAME_None;
				if (Profile.IsNone() || Merchant.AllowedWorkProfileIds.Contains(Profile)) OutError += Path + TEXT(".allowedWorkProfileIds contains an invalid or duplicate ID.\n");
				else Merchant.AllowedWorkProfileIds.Add(Profile);
			}
		}
		if (Object->HasField(TEXT("openSchedule")) && !TryReadStrictString(Object, TEXT("openSchedule"), Merchant.OpenSchedulePlaceholder))
			OutError += Path + TEXT(".openSchedule must be a string.\n");
		if (!Merchant.MerchantDefinitionId.IsNone() && OutSnapshot.Merchants.Contains(Merchant.MerchantDefinitionId)) OutError += Path + TEXT(" duplicates merchantDefinitionId.\n");
		else if (!Merchant.MerchantDefinitionId.IsNone()) OutSnapshot.Merchants.Add(Merchant.MerchantDefinitionId, Merchant);
	}

	const TArray<TSharedPtr<FJsonValue>>* Currencies = nullptr;
	if (!EconomyRoot->TryGetArrayField(TEXT("currencies"), Currencies) || Currencies->IsEmpty()) OutError += TEXT("EconomyConfig.currencies must be non-empty.\n");
	else for (const TSharedPtr<FJsonValue>& Value : *Currencies)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr; FName Id;
		if (!Object) { OutError += TEXT("EconomyConfig.currencies entries must be objects.\n"); continue; }
		if (Object && ReadId(Object, TEXT("currencyId"), Id, OutError, TEXT("currencies")))
		{
			if (OutSnapshot.CurrencyIds.Contains(Id)) OutError += TEXT("EconomyConfig contains duplicate currencyId.\n");
			OutSnapshot.CurrencyIds.Add(Id);
		}
	}
	FString CurrencyId;
	if (!TryReadStrictString(EconomyRoot, TEXT("supportedCurrencyId"), CurrencyId)
		|| (OutSnapshot.Settings.CurrencyId = NormalizeId(CurrencyId)).IsNone()) OutError += TEXT("EconomyConfig.supportedCurrencyId is required.\n");
	ReadInt64(EconomyRoot, TEXT("newProfileStartingBalance"), false, OutSnapshot.Settings.StartingBalance, OutError, TEXT("EconomyConfig"));
	FString MaximumWallet;
	if (EconomyRoot->HasField(TEXT("maximumWalletBalance")))
	{
		if (!TryReadStrictString(EconomyRoot, TEXT("maximumWalletBalance"), MaximumWallet) || MaximumWallet.IsEmpty())
		{
			OutError += TEXT("EconomyConfig.maximumWalletBalance must be a canonical base-10 string.\n");
		}
		else
		{
			FString ParseError;
			if (!ParseCanonicalNonNegativeInt64(MaximumWallet, true, OutSnapshot.Settings.MaximumWalletBalance, ParseError)) OutError += TEXT("EconomyConfig.maximumWalletBalance: ") + ParseError + TEXT("\n");
		}
	}
	ReadInt64(EconomyRoot, TEXT("maximumItemSlots"), true, OutSnapshot.Settings.MaximumItemSlots, OutError, TEXT("EconomyConfig"));
	if (OutSnapshot.Settings.StartingBalance > OutSnapshot.Settings.MaximumWalletBalance) OutError += TEXT("Starting balance exceeds maximum wallet balance.\n");
	if (!OutSnapshot.CurrencyIds.Contains(OutSnapshot.Settings.CurrencyId)) OutError += TEXT("supportedCurrencyId does not resolve.\n");
	for (const TPair<FName, FAuraOfferDefinition>& Pair : OutSnapshot.Offers)
		if (!OutSnapshot.Items.Contains(Pair.Value.ItemId)) OutError += FString::Printf(TEXT("offer '%s' references missing item '%s'.\n"), *Pair.Key.ToString(), *Pair.Value.ItemId.ToString());
	for (const TPair<FName, FAuraMerchantDefinition>& Pair : OutSnapshot.Merchants)
		for (const FName OfferId : Pair.Value.OfferIds) if (!OutSnapshot.Offers.Contains(OfferId)) OutError += FString::Printf(TEXT("merchant '%s' references missing offer '%s'.\n"), *Pair.Key.ToString(), *OfferId.ToString());
	for (const FAuraPopulationSpawnRow& Row : PopulationRows) for (const FAuraPopulationMemberOverride& Override : Row.MemberOverrides)
	{
		if (Override.MerchantDefinitionId.IsNone()) continue;
		const FName MerchantId = NormalizeId(Override.MerchantDefinitionId.ToString());
		const FAuraMerchantDefinition* Merchant = OutSnapshot.Merchants.Find(MerchantId);
		const FName WorkProfile = NormalizeId((Override.WorkProfileId.IsNone() ? Row.DefaultWorkProfileId : Override.WorkProfileId).ToString());
		if (!Merchant) OutError += FString::Printf(TEXT("population '%s:%d' references missing merchant '%s'.\n"), *Row.PopulationId.ToString(), Override.SlotIndex, *MerchantId.ToString());
		else if (!Merchant->AllowedWorkProfileIds.IsEmpty() && !Merchant->AllowedWorkProfileIds.Contains(WorkProfile))
			OutError += FString::Printf(TEXT("population '%s:%d' work profile is not allowed by merchant '%s'.\n"), *Row.PopulationId.ToString(), Override.SlotIndex, *MerchantId.ToString());
	}
	OutError.TrimEndInline();
	return OutError.IsEmpty();
}
