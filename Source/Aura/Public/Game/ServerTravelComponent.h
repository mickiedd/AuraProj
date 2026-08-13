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
 *
 * All server travel in this project should go through one of these paths:
 *   Cross-server (ClientTravel):  RouteToServerViaLoadingLevel  (or TravelToServer from LoadingPlayerController)
 *   Same-server  (OpenLevel):     RouteToMapViaLoadingLevel / RouteToMapBySoftPtrViaLoadingLevel
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class AURA_API UServerTravelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UServerTravelComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Package path of the intermediate loading level. */
	static const FString LoadingLevelPath;

	/** Package path of the login level. Used when cross-server travel fails and the
	 *  client needs to return to the login screen. */
	static const FString LoginLevelPath;

	/** Finds an existing travel component or creates/registers one on the target controller. */
	static UServerTravelComponent* GetOrCreateFor(APlayerController* InPlayerController);

	/**
	 * Updates local-controller warning and timeout thresholds.
	 */
	void ConfigureLocalTravelMonitoring(float InWarningDelaySeconds, float InTimeoutDelaySeconds);

	/**
	 * Directly travels to a dedicated server endpoint (no intermediate Loading level).
	 * Call this from ALoadingPlayerController once already on the Loading level.
	 * @param ServerEndpoint Host[:Port] endpoint.
	 * @param RequestedPlayerName Optional player name appended as ?PlayerName=...
	 */
	bool TravelToServer(const FString& ServerEndpoint, const FString& RequestedPlayerName = FString(), FName RequestedRoleId = NAME_None);

	/**
	 * Routes cross-server travel through the Loading level for this component's owning controller.
	 * Encodes the destination in the Loading level URL; LoadingPlayerController reads it on BeginPlay.
	 */
	bool TravelToServerViaLoadingLevel(const FString& ServerEndpoint, const FString& RequestedPlayerName = FString(), FName RequestedRoleId = NAME_None);

	// -----------------------------------------------------------------------
	// Static routing helpers — usable from any actor without a component ref
	// -----------------------------------------------------------------------

	/**
	 * Routes a specific client controller through the Loading level to a server endpoint.
	 * Safe to call from server authority (e.g. LevelJumpPortal GSM callback) where
	 * the controller is a remote player controller.
	 */
	static void RouteToServerViaLoadingLevel(APlayerController* InPC, const FString& ServerEndpoint, const FString& RequestedPlayerName = FString(), FName RequestedRoleId = NAME_None);

	/**
	 * Routes a same-server level travel through the Loading level.
	 * Stores the destination asset name in the GameInstance then calls OpenLevel(LoadingLevel).
	 * LoadingGameMode will continue to the real destination.
	 */
	static void RouteToMapViaLoadingLevel(UObject* WorldContextObject, const FString& MapAssetName);

	/**
	 * Same as RouteToMapViaLoadingLevel but takes a soft object pointer.
	 */
	static void RouteToMapBySoftPtrViaLoadingLevel(UObject* WorldContextObject, const TSoftObjectPtr<UWorld>& SoftMapPtr);

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
	FString BuildTravelUrl(const FString& ServerEndpoint, const FString& RequestedPlayerName, FName RequestedRoleId) const;
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
