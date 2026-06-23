// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraClientDisconnectHandler.generated.h"

class UNetDriver;
class UWorld;
class APlayerController;
class UAuraHeartbeatComponent;
class UCharacterMovementComponent;

/**
 * Client-side component that detects a mid-game loss of the dedicated server
 * and routes the player back to the Login level.
 *
 * Three independent detectors, all routing into the single OnServerLost():
 *
 *   1. Engine OnNetworkFailure (ConnectionLost/ConnectionTimeout/...) — the fast
 *      path for graceful closes; fires once the client's ServerConnection goes
 *      USOCK_Closed (UWorld::TickNetClient). The existing UServerTravelComponent
 *      only handles the initial connect window, so this covers the connected case.
 *
 *   2. Application heartbeat (UAuraHeartbeatComponent) — fires when the server
 *      stops answering ping/pong RPCs for HeartbeatTimeout. Catches process kill,
 *      crash, network drop, and hung game thread in ~seconds, even when idle.
 *
 *   3. Movement-stall watchdog — fires when the client's SavedMoves buffer stays
 *      saturated for MovementStallDuration seconds (the "Hit limit of 96 saved
 *      moves" symptom): the server is alive enough to keep the net connection open
 *      but is no longer acking gameplay/movement. Only triggers while the client is
 *      actively moving (idle clients rely on the heartbeat).
 *
 * On a qualifying loss it stashes a status message in UAuraGameInstance and
 * immediately ClientTravels to the Login level; ALoginPlayerController surfaces the
 * message on the Login screen via the existing connecting-status widget.
 *
 * Attach to the in-game client PlayerController (AAuraPlayerController).
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class AURA_API UAuraClientDisconnectHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraClientDisconnectHandler();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Detector 1: engine network-failure broadcast. */
	void HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	/** Detector 1 (robustness): engine travel-failure broadcast. */
	void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Detector 2: the heartbeat declared the server lost. */
	void HandleHeartbeatLost();

	/** Periodic check: once the server connection reaches USOCK_Open, mark us connected and arm the movement watchdog. */
	void PollConnectionState();

	/** Detector 3: declare lost when SavedMoves stays saturated for MovementStallDuration. */
	void PollMovementStall();

	/** Triggered once a qualifying mid-game loss is detected (idempotent). */
	void OnServerLost(const FString& ErrorString);

	/** Travels the owning client back to the Login level. */
	void ReturnToLogin();

	void BindFailureDelegates();
	void UnbindFailureDelegates();
	void BindHeartbeat(UAuraHeartbeatComponent* HeartbeatComp);
	void ClearTimers();
	APlayerController* GetOwningPlayerController() const;
	UCharacterMovementComponent* GetOwnedCharacterMovement() const;

	/** Returns true for failure types that mean "the server connection is gone". */
	static bool IsLossFailure(ENetworkFailure::Type FailureType);

	// ---- Config ----

	/** Message surfaced on the Login screen after the travel (may include the engine error). */
	UPROPERTY(EditDefaultsOnly, Category = "Server Lost")
	FText LostMessage;

	/** How often to sample the server connection state while waiting to become connected. */
	UPROPERTY(EditDefaultsOnly, Category = "Server Lost", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ConnectionStatePollInterval = 0.5f;

	/** SavedMoves count at/above which the movement is considered stalled (CharacterMovementComponent MAX_SAVED_MOVES is 96). */
	UPROPERTY(EditDefaultsOnly, Category = "Server Lost|Movement Watchdog", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MovementStallSavedMoveThreshold = 96;

	/** How long SavedMoves must stay saturated before declaring the server lost (avoids lag-spike false positives). */
	UPROPERTY(EditDefaultsOnly, Category = "Server Lost|Movement Watchdog", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float MovementStallDuration = 5.0f;

	/** How often to sample SavedMoves while armed. */
	UPROPERTY(EditDefaultsOnly, Category = "Server Lost|Movement Watchdog", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MovementStallPollInterval = 0.5f;

	// ---- Runtime state ----

	/** True once we observed the client's server connection reach USOCK_Open (we got into the game). */
	bool bWasConnected = false;

	/** Idempotency guard across all three detectors. */
	bool bHandled = false;

	bool bFailureDelegatesBound = false;
	bool bHeartbeatBound = false;

	FTimerHandle ConnectionPollHandle;
	FTimerHandle MovementStallHandle;

	/** FPlatformTime::Seconds() when the stall began; 0.0 while not stalling. */
	double StallSinceRealtime = 0.0;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle HeartbeatLostHandle;
};