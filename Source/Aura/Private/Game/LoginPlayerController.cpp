// Copyright Druid Mechanics

#include "Game/LoginPlayerController.h"
#include "Game/GameServerClient.h"
#include "Game/ServerTravelComponent.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Game/AuraGameInstance.h"
#include "Game/LoadScreenSaveGame.h"
#include "UI/Widget/LoginConnectingWidget.h"
#include "UI/Widget/LoginMenuWidget.h"

ALoginPlayerController::ALoginPlayerController()
{
	ServerTravelComponent = CreateDefaultSubobject<UServerTravelComponent>(TEXT("ServerTravelComponent"));

	// Use a native fallback so connection status can always render.
	ConnectingWidgetClass = ULoginConnectingWidget::StaticClass();

	// Prefer the dedicated login menu widget on the Login map unless overridden in BP.
	const FSoftClassPath DefaultLoginScreenPath(TEXT("/Game/Blueprints/UI/LoginMenu/WBP_LoginMenu.WBP_LoginMenu_C"));
	if (UClass* DefaultLoginScreenClass = DefaultLoginScreenPath.TryLoadClass<ULoginMenuWidget>())
	{
		LoginScreenWidgetClass = DefaultLoginScreenClass;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Default WBP_LoginMenu must inherit ULoginMenuWidget to be used as LoginScreenWidgetClass"));
	}
}

void ALoginPlayerController::BeginPlay()
{
	Super::BeginPlay();
	LoadServerConnectionFromJson();

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: PC=%s Local=%d AutoConnect=%d Attempted=%d WidgetClass=%s World=%s"),
		*GetNameSafe(this),
		IsLocalPlayerController() ? 1 : 0,
		bAutoConnectToServer ? 1 : 0,
		bConnectionAttempted ? 1 : 0,
		*GetNameSafe(ConnectingWidgetClass),
		*GetNameSafe(GetWorld()));

	if (IsLocalPlayerController())
	{
		if (IsValid(ServerTravelComponent))
		{
			ServerTravelComponent->ConfigureLocalTravelMonitoring(ConnectionResponseWarningDelay, ConnectionTimeoutDelay);
			ServerTravelComponent->OnStatusMessage.RemoveAll(this);
			ServerTravelComponent->OnStatusMessage.AddUObject(this, &ALoginPlayerController::HandleServerTravelStatusMessage);
		}
		EnsureLoginScreenWidget();
		SurfacePendingServerLostMessage();
		TryAutoLoginFromCommandLine();
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: manual connect mode ready (Local=%d)"),
		IsLocalPlayerController() ? 1 : 0);
}

void ALoginPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: Pawn=%s Local=%d AutoConnect=%d Attempted=%d"),
		*GetNameSafe(InPawn),
		IsLocalPlayerController() ? 1 : 0,
		bAutoConnectToServer ? 1 : 0,
		bConnectionAttempted ? 1 : 0);

	if (IsLocalPlayerController())
	{
		EnsureLoginScreenWidget();
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: manual connect mode ready (Local=%d)"),
		IsLocalPlayerController() ? 1 : 0);
}

void ALoginPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] EndPlay: reason=%d waiting=%d widget=%s"),
		static_cast<int32>(EndPlayReason),
		0,
		*GetNameSafe(ConnectingWidget));

	bQueryingGameServer = false;
	if (IsValid(ServerTravelComponent))
	{
		ServerTravelComponent->OnStatusMessage.RemoveAll(this);
	}

	if (ConnectingWidget && IsValid(ConnectingWidget))
	{
		ConnectingWidget->RemoveFromParent();
		ConnectingWidget = nullptr;
	}

	if (LoginScreenWidget && IsValid(LoginScreenWidget))
	{
		LoginScreenWidget->RemoveFromParent();
		LoginScreenWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ALoginPlayerController::EnsureLoginScreenWidget()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (LoginScreenWidget && IsValid(LoginScreenWidget))
	{
		return;
	}

	if (!LoginScreenWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] LoginScreenWidgetClass is null, Login level UI will not be shown"));
		return;
	}

	LoginScreenWidget = CreateWidget<ULoginMenuWidget>(this, LoginScreenWidgetClass);
	if (!LoginScreenWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginConn] Failed to create Login screen widget from class %s"), *GetNameSafe(LoginScreenWidgetClass));
		return;
	}

	LoginScreenWidget->AddToViewport(0);

	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LoginScreenWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (!LoginScreenWidget->InitializeForPlayerController(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Login menu widget initialization did not fully bind required UI controls"));
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Login screen widget created and shown: %s"), *GetNameSafe(LoginScreenWidget));
}

void ALoginPlayerController::ShowLoginMenuStatusMessage(const FString& InMessage)
{
	UpdateConnectingStatus(InMessage);
}

void ALoginPlayerController::SurfacePendingServerLostMessage()
{
	UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>();
	if (!IsValid(GI) || !GI->HasPendingServerLostMessage())
	{
		return;
	}

	const FString Message = GI->PendingServerLostMessage;
	GI->ClearPendingServerLostMessage();

	// Show it in the bottom-right connecting-status widget (non-blocking; the login menu stays usable).
	EnsureConnectingWidget();
	ShowLoginMenuStatusMessage(Message);
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Surfacing mid-game server-lost message on Login screen: %s"), *Message);
}

void ALoginPlayerController::HandleServerTravelStatusMessage(const FString& InMessage)
{
	UpdateConnectingStatus(InMessage);
}

void ALoginPlayerController::HandleLoginMenuSelectionChanged(const FString& SelectedDisplayName, const FString& InSelectedLevelId, int32 FallbackPort)
{
	if (SelectedDisplayName.IsEmpty() || InSelectedLevelId.IsEmpty() || FallbackPort < 1 || FallbackPort > 65535)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Login menu selection is invalid: name=%s levelId=%s fallbackPort=%d"),
			*SelectedDisplayName, *InSelectedLevelId, FallbackPort);
		return;
	}

	this->SelectedLevelId = InSelectedLevelId;
	SelectedFallbackPort = FallbackPort;
	ServerPort = FallbackPort;
	UpdateConnectingStatus(FString::Printf(TEXT("Selected server: %s (%s)"), *SelectedDisplayName, *BuildServerEndpoint()));
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Selected server target: %s -> levelId=%s fallbackPort=%d"),
		*SelectedDisplayName, *InSelectedLevelId, FallbackPort);
}

void ALoginPlayerController::RequestLoginMenuConnect(const FString& SelectedDisplayName, const FString& InSelectedLevelId, int32 FallbackPort)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (SelectedDisplayName.IsEmpty() || InSelectedLevelId.IsEmpty() || FallbackPort < 1 || FallbackPort > 65535)
	{
		EnsureConnectingWidget();
		UpdateConnectingStatus(TEXT("Select a level before connecting."));
		return;
	}

	if (bQueryingGameServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] RequestLoginMenuConnect: already querying game server, ignoring duplicate request"));
		return;
	}

	// Update SelectedLevelId, SelectedFallbackPort, and ServerPort before we capture anything.
	HandleLoginMenuSelectionChanged(SelectedDisplayName, InSelectedLevelId, FallbackPort);
	bConnectionAttempted = true;

	// Capture all values needed in the GSM callback BY VALUE before any level transition.
	// 'this' (LoginPlayerController) is destroyed when the Loading level finishes loading,
	// so it must NOT be captured in the lambda.
	const FString ResolvedPlayerName = ResolvePlayerName();
	const FString FallbackEndpoint   = BuildServerEndpoint(); // ServerAddress:FallbackPort
	const FString GSMAddress         = GameServerAddress.IsEmpty() ? ServerAddress : GameServerAddress;
	const int32   GSMPort            = GameServerPort;
	const float   QueryTimeout       = GameServerQueryTimeout;

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] RequestLoginMenuConnect: levelId='%s' gsm=%s:%d fallback='%s' player='%s'"),
		*InSelectedLevelId, *GSMAddress, GSMPort, *FallbackEndpoint, *ResolvedPlayerName);

	UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>();
	if (!IsValid(GI))
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginConn] RequestLoginMenuConnect: GameInstance is invalid, aborting"));
		return;
	}

	// Store player name in GI so ALoadingPlayerController can pick it up after
	// this controller is destroyed.
	GI->PendingCrossServerPlayerName = ResolvedPlayerName;

	// Create the GSM client on GI (as its outer) so the GC does not collect it
	// while the level transition to Loading is in progress.
	bQueryingGameServer = true;
	GameServerClient = NewObject<UGameServerClient>(GI);
	GI->PendingGameServerClient = GameServerClient;

	// Travel to the Loading level immediately — the player sees the loading screen
	// while the GSM query runs in the background.  ClientTravel is deferred one frame
	// so the RequestServer call below still fires before this PC tears down.
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] RequestLoginMenuConnect: traveling to Loading level now (GSM query in flight)"));
	ClientTravel(UServerTravelComponent::LoadingLevelPath, TRAVEL_Absolute);

	// Start the async GSM query.  Only captures GI (weak) and value types.
	TWeakObjectPtr<UAuraGameInstance> WeakGI(GI);
	GameServerClient->RequestServer(
		GSMAddress,
		GSMPort,
		InSelectedLevelId,
		QueryTimeout,
		FOnGameServerResponse::CreateLambda([WeakGI, FallbackEndpoint, ResolvedPlayerName](const FGameServerResponse& Response)
		{
			UAuraGameInstance* ResolvedGI = WeakGI.Get();
			if (!IsValid(ResolvedGI))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LoginConn] GSM callback: GameInstance is gone, cannot dispatch travel"));
				return;
			}

			// Release the GSM client so it can be GC'd.
			ResolvedGI->PendingGameServerClient = nullptr;

			FString Endpoint;
			if (Response.bSuccess)
			{
				Endpoint = FString::Printf(TEXT("%s:%d"), *Response.Host, Response.Port);
				UE_LOG(LogTemp, Display, TEXT("[LoginConn] GSM callback: success -> endpoint=%s player='%s'"),
					*Endpoint, *ResolvedPlayerName);
			}
			else if (!FallbackEndpoint.IsEmpty())
			{
				Endpoint = FallbackEndpoint;
				UE_LOG(LogTemp, Warning, TEXT("[LoginConn] GSM callback: failed (%s), using fallback endpoint=%s"),
					*Response.ErrorMessage, *Endpoint);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoginConn] GSM callback: failed with no fallback — %s"), *Response.ErrorMessage);
				ResolvedGI->ClearPendingCrossServerTravel();
				ResolvedGI->OnCrossServerTravelFailed.Broadcast(Response.ErrorMessage);
				return;
			}

			// Cache the resolved endpoint BEFORE broadcasting.  If the GSM resolves before
			// the Loading level has finished loading, ALoadingPlayerController::BeginPlay
			// will not have bound to OnCrossServerTravelReady yet and the broadcast would go
			// to zero listeners (the endpoint only existed as a lambda-local before, so it
			// was lost).  The cache lets BeginPlay consume the result directly regardless of
			// whether the callback wins or loses the race against BeginPlay.  The pending
			// flag is cleared when the result is actually consumed (in LoadingPlayerController).
			ResolvedGI->PendingCrossServerResolvedEndpoint = Endpoint;
			ResolvedGI->PendingCrossServerResolvedPlayerName = ResolvedPlayerName;
			ResolvedGI->bCrossServerTravelReady = true;
			ResolvedGI->OnCrossServerTravelReady.Broadcast(Endpoint, ResolvedPlayerName);
		}));
}

void ALoginPlayerController::EnsureConnectingWidget()
{
	if (ConnectingWidget && IsValid(ConnectingWidget))
	{
		return;
	}

	if (!ConnectingWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] ConnectingWidgetClass is null, no UI status will be shown"));
		return;
	}

	ConnectingWidget = CreateWidget<ULoginConnectingWidget>(this, ConnectingWidgetClass);
	if (!ConnectingWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginConn] Failed to create connecting widget from class %s"), *GetNameSafe(ConnectingWidgetClass));
		return;
	}

	ConnectingWidget->AddToViewport(1);
	ConnectingWidget->ShowConnecting(BuildConnectingStatusMessage());
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] connecting widget created and shown: %s"), *GetNameSafe(ConnectingWidget));
}

void ALoginPlayerController::ExecuteClientConnect()
{
	const FString ServerEndpoint = BuildServerEndpoint();

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] ExecuteClientConnect: Local=%d ServerEndpoint=%s"),
		IsLocalPlayerController() ? 1 : 0,
		*ServerEndpoint);

	if (IsLocalPlayerController())
	{
		if (ServerEndpoint.IsEmpty())
		{
			UpdateConnectingStatus(TEXT("Server configuration is invalid. Please check ServerConnection.json."));
			UE_LOG(LogTemp, Error, TEXT("[LoginConn] ExecuteClientConnect: server endpoint is empty, aborting connection"));
			return;
		}

		EnsureConnectingWidget();
		UpdateConnectingStatus(BuildConnectingStatusMessage());

		// Execute the travel command to connect to the dedicated server
		// Format: open 127.0.0.1?PlayerName=Some_Name
		const FString RequestedPlayerName = ResolvePlayerName();

		if (!IsValid(ServerTravelComponent) || !ServerTravelComponent->TravelToServerViaLoadingLevel(ServerEndpoint, RequestedPlayerName))
		{
			UpdateConnectingStatus(TEXT("Could not start connection travel. Please try again."));
			UE_LOG(LogTemp, Error, TEXT("[LoginConn] ExecuteClientConnect: ServerTravelComponent via-loading-level travel request failed"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] ExecuteClientConnect: skipped because controller is not local"));
	}
}

void ALoginPlayerController::OnGameServerResponse(const FGameServerResponse& Response)
{
	bQueryingGameServer = false;

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (Response.bSuccess)
	{
		// Use the host:port returned by the Game Server Manager.
		ServerAddress = Response.Host;
		ServerPort = Response.Port;

		UE_LOG(LogTemp, Display, TEXT("[LoginConn] Game server assigned DS at %s:%d for levelId='%s'"),
			*ServerAddress, ServerPort, *SelectedLevelId);

		EnsureConnectingWidget();
		ExecuteClientConnect();
	}
	else
	{
		// Fall back to the fixed port from LevelConfig.
		if (SelectedFallbackPort >= 1 && SelectedFallbackPort <= 65535)
		{
			ServerPort = SelectedFallbackPort;

			UE_LOG(LogTemp, Warning,
				TEXT("[LoginConn] Game server query failed (%s); falling back to fixed port %d"),
				*Response.ErrorMessage, SelectedFallbackPort);

			const FString FallbackMsg = FString::Printf(
				TEXT("Game server unavailable (%s). Connecting with fixed port %d..."),
				*Response.ErrorMessage, SelectedFallbackPort);

			EnsureConnectingWidget();
			UpdateConnectingStatus(FallbackMsg);
			ExecuteClientConnect();
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[LoginConn] Game server query failed and no valid fallback port: %s"), *Response.ErrorMessage);

			EnsureConnectingWidget();
			UpdateConnectingStatus(FString::Printf(
				TEXT("Could not reach game server: %s"), *Response.ErrorMessage));
		}
	}
}

bool ALoginPlayerController::LoadServerConnectionFromJson()
{
	TArray<FString> CandidatePaths;
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), ConnectionConfigFileName));

	FString JsonContent;
	FString LoadedFromPath;

	for (const FString& CandidatePath : CandidatePaths)
	{
		if (FPaths::FileExists(CandidatePath) && FFileHelper::LoadFileToString(JsonContent, *CandidatePath))
		{
			LoadedFromPath = CandidatePath;
			break;
		}
	}

	if (LoadedFromPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] No connection config file found (%s). Using default address=%s and selected LevelConfig port at connect time"),
			*ConnectionConfigFileName,
			*ServerAddress);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Failed to parse connection config JSON at %s. Using default address=%s and selected LevelConfig port at connect time"),
			*LoadedFromPath,
			*ServerAddress);
		return false;
	}

	FString ConfigAddress;
	if (RootObject->TryGetStringField(TEXT("serverAddress"), ConfigAddress) || RootObject->TryGetStringField(TEXT("address"), ConfigAddress))
	{
		ConfigAddress.TrimStartAndEndInline();
		if (!ConfigAddress.IsEmpty())
		{
			int32 ParsedLastColonIndex = INDEX_NONE;
			if (ConfigAddress.FindLastChar(TEXT(':'), ParsedLastColonIndex) && ParsedLastColonIndex > 0)
			{
				const FString PotentialPort = ConfigAddress.Mid(ParsedLastColonIndex + 1);
				int32 IgnoredPort = 0;
				if (LexTryParseString(IgnoredPort, *PotentialPort) && IgnoredPort >= 1 && IgnoredPort <= 65535)
				{
					UE_LOG(LogTemp, Display, TEXT("[LoginConn] Ignoring embedded port %d in serverAddress from %s; port is selected from LevelConfig."), IgnoredPort, *LoadedFromPath);
					ConfigAddress = ConfigAddress.Left(ParsedLastColonIndex);
				}
			}

			ServerAddress = ConfigAddress;
		}
	}

	if (RootObject->HasField(TEXT("serverPort")) || RootObject->HasField(TEXT("port")))
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] Ignoring serverPort/port in %s; LevelConfig selection controls the port."), *LoadedFromPath);
	}

	// Read Game Server Manager address (optional; defaults to serverAddress).
	FString ConfigGSAddress;
	if (RootObject->TryGetStringField(TEXT("gameServerAddress"), ConfigGSAddress))
	{
		ConfigGSAddress.TrimStartAndEndInline();
		if (!ConfigGSAddress.IsEmpty())
		{
			GameServerAddress = ConfigGSAddress;
		}
	}
	else
	{
		// Default game server address to the same host as the dedicated servers.
		GameServerAddress = ServerAddress;
	}

	// Read Game Server Manager port.
	double ConfigGSPort = 0.0;
	if (RootObject->TryGetNumberField(TEXT("gameServerPort"), ConfigGSPort))
	{
		const int32 ParsedGSPort = static_cast<int32>(ConfigGSPort);
		if (ParsedGSPort >= 1 && ParsedGSPort <= 65535)
		{
			GameServerPort = ParsedGSPort;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoginConn] gameServerPort %d out of range in %s; using default %d"),
				ParsedGSPort, *LoadedFromPath, GameServerPort);
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[LoginConn] Loaded server connection config from %s -> ServerAddress=%s  GameServer=%s:%d"),
		*LoadedFromPath, *ServerAddress, *GameServerAddress, GameServerPort);

	if (UAuraGameInstance* GI = Cast<UAuraGameInstance>(GetGameInstance()))
	{
		GI->GameServerAddress = GameServerAddress;
		GI->GameServerPort = GameServerPort;
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] Stored GSM config in GameInstance: %s:%d"), *GI->GameServerAddress, GI->GameServerPort);
	}

	return true;
}

FString ALoginPlayerController::ResolvePlayerName() const
{
	FString PlayerName;

	// Command-line override first. The -nullrhi auto-login launcher passes
	// -AutoLoginPlayerName=<unique> so every launched client connects as a distinct
	// player (its own pawn on the dedicated server) instead of sharing the OS username.
	FString CommandLineName;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoLoginPlayerName="), CommandLineName))
	{
		CommandLineName.TrimStartAndEndInline();
		if (!CommandLineName.IsEmpty())
		{
			PlayerName = CommandLineName;
		}
	}

	if (PlayerName.IsEmpty())
	{
		if (const UAuraGameInstance* GI = Cast<UAuraGameInstance>(GetGameInstance()))
		{
			if (!GI->LoadSlotName.IsEmpty() &&
				UGameplayStatics::DoesSaveGameExist(GI->LoadSlotName, GI->LoadSlotIndex))
			{
				if (USaveGame* SaveObject = UGameplayStatics::LoadGameFromSlot(GI->LoadSlotName, GI->LoadSlotIndex))
				{
					if (const ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveObject))
					{
						PlayerName = LoadScreenSaveGame->PlayerName;
					}
				}
			}
		}
	}

	if (PlayerName.IsEmpty()) PlayerName = FPlatformProcess::UserName(false);
	if (PlayerName.IsEmpty()) PlayerName = TEXT("Player");

	PlayerName.TrimStartAndEndInline();
	PlayerName.ReplaceInline(TEXT("?"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("&"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("="), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("#"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT(" "), TEXT("_"));

	return PlayerName;
}

FString ALoginPlayerController::BuildServerEndpoint() const
{
	FString TrimmedAddress = ServerAddress;
	TrimmedAddress.TrimStartAndEndInline();

	if (TrimmedAddress.IsEmpty())
	{
		return FString();
	}

	if (TrimmedAddress.Contains(TEXT(":")) || ServerPort <= 0)
	{
		return TrimmedAddress;
	}

	return FString::Printf(TEXT("%s:%d"), *TrimmedAddress, ServerPort);
}

FString ALoginPlayerController::BuildConnectingStatusMessage() const
{
	const FString ServerEndpoint = BuildServerEndpoint();
	if (ServerEndpoint.IsEmpty())
	{
		return TEXT("Connecting to server...");
	}

	return FString::Printf(TEXT("Connecting to %s..."), *ServerEndpoint);
}

void ALoginPlayerController::UpdateConnectingStatus(const FString& InMessage) const
{
	if (ConnectingWidget && IsValid(ConnectingWidget))
	{
		ConnectingWidget->ShowConnecting(InMessage);
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] status updated on widget: %s"), *InMessage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] status update requested but ConnectingWidget is invalid. Message=%s"), *InMessage);

		if (GEngine && IsLocalPlayerController())
		{
			const uint64 MessageKey = static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
			GEngine->AddOnScreenDebugMessage(MessageKey, 6.0f, FColor::Yellow, InMessage);
		}
	}
}

void ALoginPlayerController::TryAutoLoginFromCommandLine()
{
	// Only the local player controller on the Login map drives a connection, and only once.
	if (!IsLocalPlayerController() || bAutoLoginDispatched)
	{
		return;
	}

	const TCHAR* const CmdLine = FCommandLine::Get();

	FString RequestedLevelId;
	if (!FParse::Value(CmdLine, TEXT("AutoLoginLevel="), RequestedLevelId))
	{
		// No auto-login requested — the normal WBP_LoginMenu flow is fully in charge.
		return;
	}

	RequestedLevelId.TrimStartAndEndInline();
	if (RequestedLevelId.IsEmpty())
	{
		return;
	}

	bAutoLoginDispatched = true;

	// Optional command-line overrides, applied after LoadServerConnectionFromJson() so they win.
	FString OverrideHost;
	if (FParse::Value(CmdLine, TEXT("AutoLoginHost="), OverrideHost))
	{
		OverrideHost.TrimStartAndEndInline();
		if (!OverrideHost.IsEmpty())
		{
			ServerAddress = OverrideHost;
			GameServerAddress = OverrideHost;
			if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
			{
				GI->GameServerAddress = OverrideHost;
			}
		}
	}

	int32 OverrideGSMPort = 0;
	if (FParse::Value(CmdLine, TEXT("AutoLoginGSMPort="), OverrideGSMPort) && OverrideGSMPort >= 1 && OverrideGSMPort <= 65535)
	{
		GameServerPort = OverrideGSMPort;
		if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
		{
			GI->GameServerPort = OverrideGSMPort;
		}
	}

	int32 OverrideFallbackPort = 0;
	FParse::Value(CmdLine, TEXT("AutoLoginPort="), OverrideFallbackPort);

	float AutoLoginDelay = 0.5f;
	FString DelayString;
	if (FParse::Value(CmdLine, TEXT("AutoLoginDelay="), DelayString) && !DelayString.IsEmpty())
	{
		LexTryParseString(AutoLoginDelay, *DelayString);
		if (AutoLoginDelay < 0.0f)
		{
			AutoLoginDelay = 0.0f;
		}
	}

	// Resolve the level from LevelConfig.json the same way the menu does.
	FString DisplayName;
	FString MapPath;
	int32 LevelConfigPort = 0;
	if (!LoadLevelConfigTarget(RequestedLevelId, DisplayName, MapPath, LevelConfigPort))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] AutoLogin: level '%s' not found in LevelConfig.json; falling back to menu"), *RequestedLevelId);
		return;
	}

	const int32 FallbackPort = (OverrideFallbackPort >= 1 && OverrideFallbackPort <= 65535) ? OverrideFallbackPort : LevelConfigPort;

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] AutoLogin: level '%s' -> displayName='%s' map='%s' fallbackPort=%d (delay=%.2fs)"),
		*RequestedLevelId, *DisplayName, *MapPath, FallbackPort, AutoLoginDelay);

	// Replay the exact menu sequence: register the selection now, then connect after a short
	// delay so the world/widget are settled before ClientTravel fires to the Loading level.
	HandleLoginMenuSelectionChanged(DisplayName, RequestedLevelId, FallbackPort);

	if (UWorld* World = GetWorld())
	{
		FTimerHandle AutoLoginTimer;
		FTimerDelegate AutoLoginDelegate;
		AutoLoginDelegate.BindWeakLambda(this,
			[this, DisplayName, RequestedLevelId, FallbackPort]()
			{
				RequestLoginMenuConnect(DisplayName, RequestedLevelId, FallbackPort);
			});
		World->GetTimerManager().SetTimer(AutoLoginTimer, AutoLoginDelegate, AutoLoginDelay, false);
	}
	else
	{
		RequestLoginMenuConnect(DisplayName, RequestedLevelId, FallbackPort);
	}
}

bool ALoginPlayerController::LoadLevelConfigTarget(const FString& LevelId, FString& OutDisplayName, FString& OutMapPath, int32& OutPort)
{
	OutDisplayName.Reset();
	OutMapPath.Reset();
	OutPort = 0;

	if (LevelId.IsEmpty())
	{
		return false;
	}

	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("LevelConfig.json"));
	FString JsonContent;
	if (!FPaths::FileExists(ConfigPath) || !FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] AutoLogin: failed to read LevelConfig.json: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] AutoLogin: failed to parse LevelConfig.json: %s"), *ConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("levels"), LevelsArray) || LevelsArray == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] AutoLogin: LevelConfig.json has no 'levels' array: %s"), *ConfigPath);
		return false;
	}

	FString MatchedById;
	FString MatchedByDisplayName;
	FString MatchedByMapPath;

	for (const TSharedPtr<FJsonValue>& LevelValue : *LevelsArray)
	{
		const TSharedPtr<FJsonObject>* LevelObject = nullptr;
		if (!LevelValue.IsValid() || !LevelValue->TryGetObject(LevelObject) || LevelObject == nullptr || !LevelObject->IsValid())
		{
			continue;
		}

		FString EntryDisplayName;
		FString EntryId;
		FString EntryMapPath;
		if (!(*LevelObject)->TryGetStringField(TEXT("displayName"), EntryDisplayName) || EntryDisplayName.IsEmpty())
		{
			continue;
		}

		(*LevelObject)->TryGetStringField(TEXT("id"), EntryId);
		(*LevelObject)->TryGetStringField(TEXT("mapPath"), EntryMapPath);

		double PortValue = 0.0;
		int32 EntryPort = 0;
		if ((*LevelObject)->TryGetNumberField(TEXT("port"), PortValue))
		{
			EntryPort = static_cast<int32>(PortValue);
		}

		if (!EntryId.IsEmpty() && EntryId.Equals(LevelId, ESearchCase::IgnoreCase))
		{
			OutDisplayName = EntryDisplayName;
			OutMapPath = EntryMapPath;
			OutPort = EntryPort;
			MatchedById = EntryId;
			break;
		}

		// Tolerant fallbacks for usability; the primary match is by id.
		if (MatchedByDisplayName.IsEmpty() && EntryDisplayName.Equals(LevelId, ESearchCase::IgnoreCase))
		{
			MatchedByDisplayName = EntryDisplayName;
			OutDisplayName = EntryDisplayName;
			OutMapPath = EntryMapPath;
			OutPort = EntryPort;
		}
		else if (MatchedByDisplayName.IsEmpty() && MatchedByMapPath.IsEmpty() &&
				 !EntryMapPath.IsEmpty() && EntryMapPath.Equals(LevelId, ESearchCase::IgnoreCase))
		{
			MatchedByMapPath = EntryMapPath;
			OutDisplayName = EntryDisplayName;
			OutMapPath = EntryMapPath;
			OutPort = EntryPort;
		}
	}

	if (!MatchedById.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] AutoLogin: matched LevelConfig entry by id='%s'"), *MatchedById);
		return OutPort >= 1 && OutPort <= 65535;
	}

	if (!MatchedByDisplayName.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] AutoLogin: matched LevelConfig entry by displayName='%s'"), *MatchedByDisplayName);
		return OutPort >= 1 && OutPort <= 65535;
	}

	if (!MatchedByMapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] AutoLogin: matched LevelConfig entry by mapPath='%s'"), *MatchedByMapPath);
		return OutPort >= 1 && OutPort <= 65535;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] AutoLogin: no LevelConfig entry matched '%s'"), *LevelId);
	return false;
}
