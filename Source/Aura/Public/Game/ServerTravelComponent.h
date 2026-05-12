// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ServerTravelComponent.generated.h"

class APlayerController;
class UWorld;
class UNetDriver;

DECLARE_MULTICAST_DELEGATE_OneParam(FServerTravelStatusMessageSignature, const FString&);

/**
 * Shared client/server endpoint travel helper.
 * Attach to player controllers and route all endpoint travel through this component.
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class AURA_API UServerTravelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UServerTravelComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Finds an existing travel component or creates/registers one on the target controller. */
	static UServerTravelComponent* GetOrCreateFor(APlayerController* InPlayerController);

	/**
	 * Updates local-controller warning and timeout thresholds.
	 */
	void ConfigureLocalTravelMonitoring(float InWarningDelaySeconds, float InTimeoutDelaySeconds);

	/**
	 * Requests travel to a dedicated server endpoint.
	 * @param ServerEndpoint Host[:Port] endpoint.
	 * @param RequestedPlayerName Optional player name that will be appended as ?PlayerName=...
	 */
	bool TravelToServer(const FString& ServerEndpoint, const FString& RequestedPlayerName = FString());

	FServerTravelStatusMessageSignature OnStatusMessage;

private:
	void HandleConnectionResponseWarning();
	void HandleConnectionTimeout();
	void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void BindConnectionFailureDelegates();
	void UnbindConnectionFailureDelegates();
	void ClearMonitoringTimers();
	void BroadcastStatusMessage(const FString& InMessage) const;
	FString BuildTravelUrl(const FString& ServerEndpoint, const FString& RequestedPlayerName) const;
	static FString SanitizePlayerName(FString PlayerName);
	APlayerController* GetOwningPlayerController() const;

	bool bWaitingForConnectionResponse = false;
	bool bFailureDelegatesBound = false;
	float ConnectionResponseWarningDelay = 5.0f;
	float ConnectionTimeoutDelay = 12.0f;
	FString ActiveServerEndpoint;

	FTimerHandle ConnectionResponseWarningTimerHandle;
	FTimerHandle ConnectionTimeoutTimerHandle;
};
