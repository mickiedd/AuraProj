// Copyright Druid Mechanics

#include "Network/AuraNetDriver.h"
#include "Network/AuraNetConnection.h"
#include "Aura/AuraLogChannels.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"

bool UAuraNetDriver::InitConnectionClass()
{
	const bool bSuperResult = Super::InitConnectionClass();

	// The base implementation loads NetConnectionClass from the
	// NetConnectionClassName config entry (set in [/Script/Aura.AuraNetDriver]).
	// Guarantee our class regardless of config so the driver->connection wiring
	// is authoritative from the business layer.
	if (NetConnectionClass != UAuraNetConnection::StaticClass())
	{
		NetConnectionClass = UAuraNetConnection::StaticClass();
	}

	UE_LOG(LogAura, Log,
		TEXT("[NetDriver] InitConnectionClass: superOk=%s connectionClass=%s"),
		bSuperResult ? TEXT("true") : TEXT("false"),
		*GetNameSafe(NetConnectionClass));

	return NetConnectionClass != nullptr;
}

bool UAuraNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	const bool bResult = Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);

	UE_LOG(LogAura, Log,
		TEXT("[NetDriver] InitBase: asClient=%s url=%s reuse=%s result=%s error=%s"),
		bInitAsClient ? TEXT("true") : TEXT("false"),
		*URL.ToString(),
		bReuseAddressAndPort ? TEXT("true") : TEXT("false"),
		bResult ? TEXT("true") : TEXT("false"),
		*Error);

	return bResult;
}

bool UAuraNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	const bool bResult = Super::InitConnect(InNotify, ConnectURL, Error);

	UE_LOG(LogAura, Log,
		TEXT("[NetDriver] InitConnect: url=%s result=%s error=%s"),
		*ConnectURL.ToString(),
		bResult ? TEXT("true") : TEXT("false"),
		*Error);

	return bResult;
}

bool UAuraNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	const bool bResult = Super::InitListen(InNotify, LocalURL, bReuseAddressAndPort, Error);

	UE_LOG(LogAura, Log,
		TEXT("[NetDriver] InitListen: url=%s reuse=%s result=%s error=%s"),
		*LocalURL.ToString(),
		bReuseAddressAndPort ? TEXT("true") : TEXT("false"),
		bResult ? TEXT("true") : TEXT("false"),
		*Error);

	return bResult;
}

void UAuraNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);
}

void UAuraNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	Super::LowLevelSend(Address, Data, CountBits, Traits);

	UE_LOG(LogAura, VeryVerbose,
		TEXT("[NetDriver] LowLevelSend: addr=%s bits=%d bytes=%d"),
		Address.IsValid() ? *Address->ToString(true) : TEXT("<none>"),
		CountBits,
		(CountBits + 7) / 8);
}

FString UAuraNetDriver::LowLevelGetNetworkNumber()
{
	const FString Result = Super::LowLevelGetNetworkNumber();
	UE_LOG(LogAura, Verbose, TEXT("[NetDriver] LowLevelGetNetworkNumber -> %s"), *Result);
	return Result;
}

void UAuraNetDriver::LowLevelDestroy()
{
	UE_LOG(LogAura, Log, TEXT("[NetDriver] LowLevelDestroy"));
	Super::LowLevelDestroy();
}