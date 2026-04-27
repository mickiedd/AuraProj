// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerController.h"
#include "LoginPlayerController.generated.h"

/**
 * Player controller for the Login map.
 * Handles client-side auto-connection to dedicated server.
 */

class ALoginGameMode;
class ULoginConnectingWidget;
class UWorld;
class UNetDriver;

/**
 * Player controller for the Login map.
 * Handles client-side auto-connection to dedicated server.
 */
UCLASS()
class AURA_API ALoginPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

protected:
	/**
	 * Server host or IP to connect to.
	 * Example: 127.0.0.1
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	FString ServerAddress = TEXT("127.0.0.1");

	/**
	 * Server port to connect to.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection", meta=(ClampMin="1", ClampMax="65535"))
	int32 ServerPort = 7777;

	/**
	 * JSON file name searched under Saved/Config first, then Config.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	FString ConnectionConfigFileName = TEXT("ServerConnection.json");

	/**
	 * Whether to auto-connect on this controller.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	bool bAutoConnectToServer = true;

	/**
	 * Execute the auto-connect command with a delay.
	 */
	UFUNCTION()
	void ExecuteClientConnect();

	UFUNCTION()
	void HandleConnectionResponseWarning();

	UFUNCTION()
	void HandleConnectionTimeout();

	void UpdateConnectingStatus(const FString& InMessage) const;

	void BindConnectionFailureDelegates();
	void UnbindConnectionFailureDelegates();

	bool LoadServerConnectionFromJson();
	FString BuildServerEndpoint() const;
	FString BuildConnectingStatusMessage() const;

	/**
	 * Flag to ensure we only attempt connection once.
	 */
	bool bConnectionAttempted = false;

	/**
	 * True while waiting for a successful map travel or a failure callback.
	 */
	bool bWaitingForConnectionResponse = false;

	/**
	 * Tracks whether global engine delegates were bound by this controller.
	 */
	bool bFailureDelegatesBound = false;

	/**
	 * Widget class to display connection status.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Connection")
	TSubclassOf<ULoginConnectingWidget> ConnectingWidgetClass;

	/**
	 * Instance of the connecting widget.
	 */
	UPROPERTY()
	TObjectPtr<ULoginConnectingWidget> ConnectingWidget;

	/**
	 * Timer handle for the connection delay.
	 */
	FTimerHandle ConnectionTimerHandle;

	/**
	 * Timer handle used to show a "still connecting" hint.
	 */
	FTimerHandle ConnectionResponseWarningTimerHandle;

	/**
	 * Timer handle used for connection timeout messaging.
	 */
	FTimerHandle ConnectionTimeoutTimerHandle;

	/**
	 * Seconds before showing an additional connecting hint.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection", meta=(ClampMin="1.0"))
	float ConnectionResponseWarningDelay = 5.0f;

	/**
	 * Seconds before showing timeout text if no connection response arrives.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection", meta=(ClampMin="2.0"))
	float ConnectionTimeoutDelay = 12.0f;
};
