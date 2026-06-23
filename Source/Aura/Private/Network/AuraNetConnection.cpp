// Copyright Druid Mechanics

#include "Network/AuraNetConnection.h"
#include "Aura/AuraLogChannels.h"
#include "Engine/NetConnection.h"

UAuraNetConnection::UAuraNetConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAuraNetConnection::InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	UE_LOG(LogAura, Log,
		TEXT("[NetConn] InitBase: driver=%s url=%s state=%d maxPacket=%d overhead=%d"),
		*GetNameSafe(InDriver),
		*InURL.ToString(),
		static_cast<int32>(InState),
		InMaxPacket,
		InPacketOverhead);
}

void UAuraNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, const FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);

	UE_LOG(LogAura, Log,
		TEXT("[NetConn] InitRemoteConnection (server-side): driver=%s remote=%s url=%s state=%d"),
		*GetNameSafe(InDriver),
		*InRemoteAddr.ToString(true),
		*InURL.ToString(),
		static_cast<int32>(InState));
}

void UAuraNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	UE_LOG(LogAura, Log,
		TEXT("[NetConn] InitLocalConnection (client-side): driver=%s url=%s state=%d"),
		*GetNameSafe(InDriver),
		*InURL.ToString(),
		static_cast<int32>(InState));
}

void UAuraNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	Super::LowLevelSend(Data, CountBits, Traits);

	UE_LOG(LogAura, VeryVerbose, TEXT("[NetConn] LowLevelSend: bits=%d bytes=%d"), CountBits, (CountBits + 7) / 8);
}

FString UAuraNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	const FString Result = Super::LowLevelGetRemoteAddress(bAppendPort);
	UE_LOG(LogAura, Verbose, TEXT("[NetConn] LowLevelGetRemoteAddress(bAppendPort=%d) -> %s"), bAppendPort ? 1 : 0, *Result);
	return Result;
}

FString UAuraNetConnection::LowLevelDescribe()
{
	const FString Result = Super::LowLevelDescribe();
	UE_LOG(LogAura, Verbose, TEXT("[NetConn] LowLevelDescribe -> %s"), *Result);
	return Result;
}

void UAuraNetConnection::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void UAuraNetConnection::CleanUp()
{
	UE_LOG(LogAura, Log, TEXT("[NetConn] CleanUp: remote=%s"), *LowLevelGetRemoteAddress(true));
	Super::CleanUp();
}

void UAuraNetConnection::ReceivedRawPacket(void* Data, int32 Count)
{
	Super::ReceivedRawPacket(Data, Count);

	UE_LOG(LogAura, VeryVerbose, TEXT("[NetConn] ReceivedRawPacket: bytes=%d"), Count);
}

float UAuraNetConnection::GetTimeoutValue()
{
	return Super::GetTimeoutValue();
}