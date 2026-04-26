// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LoginPlayerController.generated.h"

/**
 * Player controller for the Login map.
 * Handles client-side auto-connection to dedicated server.
 */

class ALoginGameMode;
class ULoginConnectingWidget;

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

protected:
	/**
	 * Server address to connect to.
	 * Format: 127.0.0.1:7777 or 127.0.0.1
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	FString ServerAddress = TEXT("127.0.0.1");

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

	/**
	 * Flag to ensure we only attempt connection once.
	 */
	bool bConnectionAttempted = false;

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
	 * Timer handle for hiding the connection widget after travel starts.
	 */
	FTimerHandle HideConnectingWidgetTimerHandle;
};
