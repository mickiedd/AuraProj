// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "IpConnection.h"
#include "AuraNetConnection.generated.h"

/**
 * Aura-owned network connection.
 *
 * Business-logic subclass of the engine UIpConnection. Every override here is a
 * pass-through that calls Super:: and emits an LogAura hook line, so it does NOT
 * change networking behavior on its own — it only provides an extension surface
 * (custom handshake, per-connection metrics/limits, packet inspection, ...) for
 * later work without touching engine code.
 *
 * It is wired into UAuraNetDriver via UAuraNetDriver::InitConnectionClass() and
 * the [/Script/Aura.AuraNetDriver] NetConnectionClassName config entry.
 */
UCLASS(transient, config=Engine)
class AURA_API UAuraNetConnection : public UIpConnection
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UIpConnection / UNetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CleanUp() override;
	virtual void ReceivedRawPacket(void* Data, int32 Count) override;
	virtual float GetTimeoutValue() override;
	//~ End UIpConnection / UNetConnection Interface
};