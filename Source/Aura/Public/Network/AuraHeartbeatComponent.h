// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraHeartbeatComponent.generated.h"

class APlayerController;

/**
 * Application-level heartbeat for detecting a lost/hung dedicated server faster
 * and more reliably than the engine's 60s no-packet ConnectionTimeout.
 *
 * The owning client pings a Server RPC on a fixed interval; the server answers with
 * a Client RPC pong. The client arms a watchdog on the first pong and declares the
 * server lost (broadcasts OnHeartbeatLost) when no pong has arrived for
 * HeartbeatTimeout seconds.
 *
 * Catches: process kill, crash, network drop, hung game thread (RPCs stop being
 * processed) — even while the client is idle. Does NOT catch "server alive and still
 * answering RPCs but not acking movement"; that is covered by the movement-stall
 * watchdog in UAuraClientDisconnectHandler.
 *
 * Attach to the in-game client PlayerController (AAuraPlayerController). Replicated
 * so the server instance can answer pings.
 */
DECLARE_MULTICAST_DELEGATE(FOnHeartbeatLostSignature);

UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class AURA_API UAuraHeartbeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraHeartbeatComponent();

	/** Fired on the owning client when no pong has been received within HeartbeatTimeout. */
	FOnHeartbeatLostSignature OnHeartbeatLost;

	// ---- RPCs ----

	/** Client -> server: a ping. Unreliable so dropped pings just cost one round. */
	UFUNCTION(Server, Unreliable)
	void ServerHeartbeat(int32 Sequence);

	/** Server -> owning client: the pong. Unreliable; the timeout tolerates several misses. */
	UFUNCTION(Client, Unreliable)
	void ClientHeartbeatAck(int32 Sequence);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Only the owning client ticks these (the server only answers RPCs). */
	bool IsOwningClient() const;

	void SendHeartbeat();
	void CheckHeartbeat();
	void ClearTimers();

	// ---- Config ----

	UPROPERTY(EditDefaultsOnly, Category = "Heartbeat", meta = (ClampMin = "0.2", ClampMax = "10.0"))
	float HeartbeatInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Heartbeat", meta = (ClampMin = "2.0", ClampMax = "60.0"))
	float HeartbeatTimeout = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Heartbeat", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float WatchdogTickInterval = 0.5f;

	// ---- Runtime state ----

	/** True once the first pong has arrived (round-trip confirmed). */
	bool bArmed = false;

	int32 NextSequence = 0;

	/** FPlatformTime::Seconds() of the last received pong. */
	double LastPongRealtime = 0.0;

	FTimerHandle SendHandle;
	FTimerHandle CheckHandle;
};