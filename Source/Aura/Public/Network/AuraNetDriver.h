// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "AuraNetDriver.generated.h"

/**
 * Aura-owned network driver.
 *
 * Business-logic subclass of the engine UIpNetDriver. It is repointed onto the
 * GameNetDriver definition in Config/DefaultEngine.ini so it replaces the
 * engine IpNetDriver for every client/server connection, while keeping IpNetDriver
 * as the construction fallback.
 *
 * Its only responsibilities here are:
 *   - InitConnectionClass(): wire the driver to UAuraNetConnection (guaranteed,
 *     independent of stray config).
 *   - Thin pass-through overrides that call Super:: and emit an LogAura hook line,
 *     giving an extension surface for later work (custom handshake, metrics/limits,
 *     packet inspection, ...) without changing networking behavior.
 */
UCLASS(transient, config=Engine)
class AURA_API UAuraNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	//~ Begin UNetDriver / UIpNetDriver Interface
	virtual bool InitConnectionClass() override;
	virtual bool InitBase(bool bInitAsClient, class FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(class FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(class FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual void LowLevelDestroy() override;
	//~ End UNetDriver / UIpNetDriver Interface
};