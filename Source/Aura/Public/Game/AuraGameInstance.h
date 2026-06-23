// Copyright Druid Mechanics
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/GameServerClient.h"
#include "AuraGameInstance.generated.h"

/**
 * Fired when a GSM query (started from the Login screen) resolves successfully
 * or falls back to a fixed port.  ALoadingPlayerController binds to this.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCrossServerTravelReadySignature, const FString& /*Endpoint*/, const FString& /*PlayerName*/);

/**
 * Fired when a GSM query fails with no usable fallback.
 * ALoadingPlayerController uses this to navigate back to the Login level.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCrossServerTravelFailedSignature, const FString& /*ErrorMessage*/);

/**
 * 
 */
UCLASS()
class AURA_API UAuraGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:

	UPROPERTY()
	FName PlayerStartTag = FName();

	UPROPERTY()
	FString LoadSlotName = FString();

	UPROPERTY()
	int32 LoadSlotIndex = 0;

	// ---------------------------------------------------------------------------
	// Pending same-server map travel (set before OpenLevel to the Loading map)
	// ---------------------------------------------------------------------------

	/** Full package path or short asset name of the map to open after the Loading level. */
	UPROPERTY()
	FString PendingMapAssetName;

	/** Soft pointer alternative when available (takes precedence over PendingMapAssetName). */
	UPROPERTY()
	TSoftObjectPtr<UWorld> PendingMapSoftPtr;

	bool HasPendingMapTravel() const { return !PendingMapSoftPtr.IsNull() || !PendingMapAssetName.IsEmpty(); }

	void ClearPendingMapTravel()
	{
		PendingMapSoftPtr.Reset();
		PendingMapAssetName.Empty();
	}

	// ---------------------------------------------------------------------------
	// Pending cross-server travel via Login-screen GSM query
	//
	// The LoginPlayerController is destroyed when it travels to the Loading level,
	// so the active GSM client and its result are held here for ALoadingPlayerController
	// to pick up after the level transition.
	// ---------------------------------------------------------------------------

	/** Keeps the active GSM client alive across the level transition to Loading. */
	UPROPERTY()
	TObjectPtr<UGameServerClient> PendingGameServerClient;

	/** Player name resolved before traveling to Loading.  Non-empty = login GSM still in flight. */
	UPROPERTY()
	FString PendingCrossServerPlayerName;

	/** Game Server Manager address and port, loaded from ServerConnection.json. */
	UPROPERTY()
	FString GameServerAddress = FString();

	UPROPERTY()
	int32 GameServerPort = 9000;

	// ---------------------------------------------------------------------------
	// Pending portal-triggered GSM query (Loading map starts this on the client)
	// ---------------------------------------------------------------------------

	/** Portal destination level id to resolve through GSM once the client reaches Loading. */
	UPROPERTY()
	FString PendingPortalServerId;

	/** Optional fallback endpoint if GSM fails. */
	UPROPERTY()
	FString PendingPortalFallbackEndpoint;

	/** Player name to append to the eventual ClientTravel. */
	UPROPERTY()
	FString PendingPortalPlayerName;

	bool HasPendingPortalTravel() const { return !PendingPortalServerId.IsEmpty(); }

	void ClearPendingPortalTravel()
	{
		PendingPortalServerId.Empty();
		PendingPortalFallbackEndpoint.Empty();
		PendingPortalPlayerName.Empty();
	}

	/** Fired when the GSM query resolves (success or fallback). */
	FCrossServerTravelReadySignature OnCrossServerTravelReady;

	/** Fired when the GSM query fails with no fallback. */
	FCrossServerTravelFailedSignature OnCrossServerTravelFailed;

	bool HasPendingCrossServerTravel() const { return !PendingCrossServerPlayerName.IsEmpty(); }

	void ClearPendingCrossServerTravel()
	{
		PendingCrossServerPlayerName.Empty();
		PendingGameServerClient = nullptr;
	}

	// ---------------------------------------------------------------------------
	// Mid-game server-lost handoff
	//
	// When UAuraClientDisconnectHandler detects that the dedicated server went
	// away mid-game, it immediately travels back to the Login level and stashes
	// this message so ALoginPlayerController can surface it on the Login screen.
	// ---------------------------------------------------------------------------

	UPROPERTY()
	FString PendingServerLostMessage;

	bool HasPendingServerLostMessage() const { return !PendingServerLostMessage.IsEmpty(); }

	void ClearPendingServerLostMessage() { PendingServerLostMessage.Empty(); }
};
