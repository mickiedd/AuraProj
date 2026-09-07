// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameServerClient.generated.h"

/**
 * Response received from the Game Server Manager.
 * bSuccess == true  -> Host and Port are valid and the dedicated server is ready.
 * bSuccess == false -> ErrorMessage describes what went wrong.
 */
USTRUCT(BlueprintType)
struct AURA_API FGameServerResponse
{
	GENERATED_BODY()

	/** Whether the request succeeded and the server is ready. */
	UPROPERTY(BlueprintReadOnly, Category = "GameServer")
	bool bSuccess = false;

	/** A parsed manager response is authoritative, including a startup rejection. */
	bool bReceivedManagerResponse = false;

	/** Hostname or IP of the dedicated server (valid when bSuccess is true). */
	UPROPERTY(BlueprintReadOnly, Category = "GameServer")
	FString Host;

	/** Port of the dedicated server (valid when bSuccess is true). */
	UPROPERTY(BlueprintReadOnly, Category = "GameServer")
	int32 Port = 0;

	/** Human-readable failure reason (valid when bSuccess is false). */
	UPROPERTY(BlueprintReadOnly, Category = "GameServer")
	FString ErrorMessage;
};

/**
 * Delegate fired on the game thread when a Game Server Manager query completes.
 * @param Response  Result of the request; check Response.bSuccess first.
 */
DECLARE_DELEGATE_OneParam(FOnGameServerResponse, const FGameServerResponse& /*Response*/);

/**
 * Sends a level-request to the Aura Game Server Manager over TCP and returns
 * the assigned dedicated-server endpoint via delegate.
 *
 * The TCP operation runs on a thread-pool thread so the game thread is never
 * blocked.  The delegate is always fired on the game thread.
 *
 * Typical usage:
 *   UGameServerClient* Client = NewObject<UGameServerClient>(this);
 *   Client->RequestServer(Addr, Port, LevelId, 30.f,
 *       FOnGameServerResponse::CreateUObject(this, &ALoginPlayerController::OnGameServerResponse));
 */
UCLASS()
class AURA_API UGameServerClient : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Asynchronously query the Game Server Manager for a dedicated server.
	 *
	 * @param GameServerAddress  Hostname or IP of the Game Server Manager.
	 * @param GameServerPort     TCP port the Game Server Manager listens on.
	 * @param LevelId            The level id to request (matches LevelConfig.json "id").
	 * @param TimeoutSeconds     Per-operation socket timeout (connect + read).
	 *                           The server may need startup grace time; use >= 30s.
	 * @param OnResponse         Callback fired on the game thread with the result.
	 */
	void RequestServer(
		const FString& GameServerAddress,
		int32 GameServerPort,
		const FString& LevelId,
		float TimeoutSeconds,
		FOnGameServerResponse OnResponse);
};
