// Copyright Druid Mechanics

#include "Game/AuraPlayerProfileIdentity.h"

#include "GameFramework/OnlineReplStructs.h"
#include "Misc/Crc.h"

namespace AuraPlayerProfileIdentityPrivate
{
	FString Normalize(const FString& Value)
	{
		FString Result = Value;
		Result.TrimStartAndEndInline();
		Result.ToLowerInline();
		return Result;
	}

	bool HasUnsafeCharacters(const FString& Value)
	{
		return Value.Contains(TEXT("/")) || Value.Contains(TEXT("\\")) || Value.Contains(TEXT(".."))
			|| Value.Contains(TEXT("?")) || Value.Contains(TEXT("&")) || Value.Contains(TEXT("="));
	}
}

bool FAuraPlayerProfileId::IsValid() const
{
	return !ProviderName.IsEmpty() && !UniqueId.IsEmpty()
		&& !AuraPlayerProfileIdentityPrivate::HasUnsafeCharacters(ProviderName)
		&& !AuraPlayerProfileIdentityPrivate::HasUnsafeCharacters(UniqueId);
}

FString FAuraPlayerProfileId::ToCanonicalString() const
{
	return AuraPlayerProfileIdentityPrivate::Normalize(ProviderName) + TEXT(":")
		+ AuraPlayerProfileIdentityPrivate::Normalize(UniqueId);
}

FString FAuraPlayerProfileId::BuildSlotName(const FString& WorldPersistenceId) const
{
	const uint32 IdentityHash = FCrc::StrCrc32(*ToCanonicalString());
	const uint32 WorldHash = FCrc::StrCrc32(*AuraPlayerProfileIdentityPrivate::Normalize(WorldPersistenceId));
	return FString::Printf(TEXT("AuraProfile_%08X_%08X"), WorldHash, IdentityHash);
}

bool FAuraPlayerProfileId::ValidateComponents(const FString& InProviderName, const FString& InUniqueId,
	const FString& ExpectedProviderName, bool bAllowDevelopmentFixture, FAuraPlayerProfileId& OutId, FString& OutError)
{
	OutId = FAuraPlayerProfileId();
	OutError.Reset();
	const FString Provider = AuraPlayerProfileIdentityPrivate::Normalize(InProviderName);
	const FString Value = InUniqueId.TrimStartAndEnd();
	if (Provider.IsEmpty() || Value.IsEmpty())
	{
		OutError = TEXT("Authenticated provider identity is missing.");
		return false;
	}
	if (ExpectedProviderName.IsEmpty() || Provider != AuraPlayerProfileIdentityPrivate::Normalize(ExpectedProviderName))
	{
		OutError = FString::Printf(TEXT("Identity provider mismatch: received '%s', expected '%s'."), *Provider, *ExpectedProviderName);
		return false;
	}
	if (AuraPlayerProfileIdentityPrivate::HasUnsafeCharacters(Value))
	{
		OutError = TEXT("Authenticated identity contains unsafe path or URL characters.");
		return false;
	}
	if (Provider.Equals(TEXT("NULL"), ESearchCase::IgnoreCase) && !bAllowDevelopmentFixture)
	{
		OutError = TEXT("OnlineSubsystemNull identities are local test identities and cannot satisfy persistent login.");
		return false;
	}
	OutId.ProviderName = Provider;
	OutId.UniqueId = Value;
	return OutId.IsValid();
}

bool FAuraPlayerProfileId::FromUniqueNetId(const FUniqueNetIdRepl& NetId, const FString& ExpectedProviderName,
	bool bAllowDevelopmentFixture, FAuraPlayerProfileId& OutId, FString& OutError)
{
	if (!NetId.IsValid() || !NetId.GetUniqueNetId().IsValid())
	{
		OutId = FAuraPlayerProfileId();
		OutError = TEXT("Authenticated unique net ID is invalid or absent.");
		return false;
	}
	return ValidateComponents(NetId.GetType().ToString(), NetId.ToString(), ExpectedProviderName,
		bAllowDevelopmentFixture, OutId, OutError);
}
