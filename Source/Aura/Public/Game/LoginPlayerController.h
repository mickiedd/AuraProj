// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LoginPlayerController.generated.h"

/**
 * Player controller for the Login map.
 * Handles client-side auto-connection to dedicated server.
 */

class UGameServerClient;
class ULoginConnectingWidget;
class ULoginMenuWidget;
class UServerTravelComponent;
struct FGameServerResponse;

/**
 * Player controller for the Login map.
 * Handles client-side auto-connection to dedicated server.
 */
UCLASS()
class AURA_API ALoginPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALoginPlayerController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ShowLoginMenuStatusMessage(const FString& InMessage);

	/**
	 * Called when the user selects a level in the login menu.
	 * @param SelectedDisplayName  Human-readable level name.
	 * @param SelectedLevelId      Level id string (from LevelConfig.json "id" field).
	 * @param FallbackPort         Port from LevelConfig used if the game server is unreachable.
	 */
	void HandleLoginMenuSelectionChanged(const FString& SelectedDisplayName, const FString& SelectedLevelId, int32 FallbackPort);

	/**
	 * Called when the user clicks Connect.
	 * Queries the Game Server Manager for the dedicated server endpoint, then connects.
	 * Falls back to the fixed port from LevelConfig if the game server is unreachable.
	 */
	void RequestLoginMenuConnect(const FString& SelectedDisplayName, const FString& SelectedLevelId, int32 FallbackPort);

protected:
	/**
	 * Dedicated server host or IP to connect to.
	 * Set from ServerConnection.json at runtime; defaults to 127.0.0.1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	FString ServerAddress = TEXT("127.0.0.1");

	/**
	 * Dedicated server port.
	 * Overwritten at runtime with the port returned by the Game Server Manager
	 * (or the LevelConfig fallback port when the game server is unreachable).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection", meta=(ClampMin="1", ClampMax="65535"))
	int32 ServerPort = 7777;

	/**
	 * Hostname or IP of the Game Server Manager.
	 * Defaults to the same address as ServerAddress; can be overridden in
	 * ServerConnection.json via "gameServerAddress".
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Game Server Manager")
	FString GameServerAddress = TEXT("127.0.0.1");

	/**
	 * TCP port the Game Server Manager listens on.
	 * Read from ServerConnection.json "gameServerPort"; defaults to 9000.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Game Server Manager", meta=(ClampMin="1", ClampMax="65535"))
	int32 GameServerPort = 9000;

	/**
	 * Timeout (seconds) for the TCP query to the Game Server Manager.
	 * Should be long enough to cover dedicated server startup grace time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Game Server Manager", meta=(ClampMin="5.0", ClampMax="120.0"))
	float GameServerQueryTimeout = 35.0f;

	/**
	 * JSON file name searched under Saved/Config first, then Config.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	FString ConnectionConfigFileName = TEXT("ServerConnection.json");

	/**
	 * Legacy flag kept for compatibility. Login now connects only through the menu.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Server Connection")
	bool bAutoConnectToServer = false;

	/**
	 * Execute the auto-connect command with a delay.
	 */
	UFUNCTION()
	void ExecuteClientConnect();

	void UpdateConnectingStatus(const FString& InMessage) const;
	void HandleServerTravelStatusMessage(const FString& InMessage);
	void EnsureConnectingWidget();
	void EnsureLoginScreenWidget();

	/** If the GameInstance carries a mid-game server-lost message (set by
	 *  UAuraClientDisconnectHandler before traveling here), surface it on the
	 *  Login screen via the connecting-status widget and clear it. */
	void SurfacePendingServerLostMessage();

	/** Callback from UGameServerClient fired on the game thread. */
	void OnGameServerResponse(const FGameServerResponse& Response);

	/** Resolves the player name from save data, OS username, or a default.
	 *  Safe to call before traveling to the Loading level. */
	FString ResolvePlayerName() const;

	bool LoadServerConnectionFromJson();
	FString BuildServerEndpoint() const;
	FString BuildConnectingStatusMessage() const;

	/**
	 * Flag to ensure we only attempt connection once.
	 */
	bool bConnectionAttempted = false;

	/** True while a Game Server Manager TCP query is in flight. */
	bool bQueryingGameServer = false;

	/** Level id selected by the user; sent to the Game Server Manager. */
	FString SelectedLevelId;

	/** Fallback port from LevelConfig, used if the game server is unreachable. */
	int32 SelectedFallbackPort = 0;

	/**
	 * Widget class to display connection status.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Connection")
	TSubclassOf<ULoginConnectingWidget> ConnectingWidgetClass;

	/**
	 * Widget class shown for the Login level (for example WBP_MainMenu).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|UI")
	TSubclassOf<ULoginMenuWidget> LoginScreenWidgetClass;

	/**
	 * Runtime instance of the Login screen widget.
	 */
	UPROPERTY()
	TObjectPtr<ULoginMenuWidget> LoginScreenWidget;

	/**
	 * Instance of the connecting widget.
	 */
	UPROPERTY()
	TObjectPtr<ULoginConnectingWidget> ConnectingWidget;

	/** Active game server TCP query client. Replaced on each connect attempt. */
	UPROPERTY()
	TObjectPtr<UGameServerClient> GameServerClient;

	/** Shared component used by all endpoint server travel operations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login|Server Connection")
	TObjectPtr<UServerTravelComponent> ServerTravelComponent;

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
