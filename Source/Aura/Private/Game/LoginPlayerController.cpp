// Copyright Druid Mechanics

#include "Game/LoginPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Widget.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Containers/Array.h"
#include "Game/AuraGameInstance.h"
#include "Game/LoadScreenSaveGame.h"
#include "Game/LoginGameMode.h"
#include "UI/Widget/LoginConnectingWidget.h"

ALoginPlayerController::ALoginPlayerController()
{
	// Use a native fallback so connection status can always render.
	ConnectingWidgetClass = ULoginConnectingWidget::StaticClass();

	// Prefer the dedicated login menu widget on the Login map unless overridden in BP.
	const FSoftClassPath DefaultLoginScreenPath(TEXT("/Game/Blueprints/UI/LoginMenu/WBP_LoginMenu.WBP_LoginMenu_C"));
	if (UClass* DefaultLoginScreenClass = DefaultLoginScreenPath.TryLoadClass<UUserWidget>())
	{
		LoginScreenWidgetClass = DefaultLoginScreenClass;
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
		BindConnectionFailureDelegates();
		EnsureLoginScreenWidget();
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: manual connect mode ready (Local=%d Manual=%d)"),
		IsLocalPlayerController() ? 1 : 0,
		bUseLoginMenuManualConnect ? 1 : 0);
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

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: manual connect mode ready (Local=%d Manual=%d)"),
		IsLocalPlayerController() ? 1 : 0,
		bUseLoginMenuManualConnect ? 1 : 0);
}

void ALoginPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] EndPlay: reason=%d waiting=%d widget=%s"),
		static_cast<int32>(EndPlayReason),
		bWaitingForConnectionResponse ? 1 : 0,
		*GetNameSafe(ConnectingWidget));

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(ConnectionTimerHandle);
		TimerManager.ClearTimer(ConnectionResponseWarningTimerHandle);
		TimerManager.ClearTimer(ConnectionTimeoutTimerHandle);
	}

	bWaitingForConnectionResponse = false;
	UnbindConnectionFailureDelegates();

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

	LoginLevelComboBox = nullptr;
	LoginConnectButton = nullptr;
	AvailableServerTargets.Reset();
	bUseLoginMenuManualConnect = false;

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

	LoginScreenWidget = CreateWidget<UUserWidget>(this, LoginScreenWidgetClass);
	if (!LoginScreenWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginConn] Failed to create Login screen widget from class %s"), *GetNameSafe(LoginScreenWidgetClass));
		return;
	}

	LoginScreenWidget->AddToViewport(0);
	bUseLoginMenuManualConnect = InitializeLoginMenuBindings();
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Login screen widget created and shown: %s"), *GetNameSafe(LoginScreenWidget));
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

bool ALoginPlayerController::InitializeLoginMenuBindings()
{
	LoginLevelComboBox = nullptr;
	LoginConnectButton = nullptr;

	if (!LoginScreenWidget || !LoginScreenWidget->WidgetTree)
	{
		return false;
	}

	LoginLevelComboBox = Cast<UComboBoxString>(LoginScreenWidget->WidgetTree->FindWidget(TEXT("ComboBoxList")));
	LoginConnectButton = Cast<UButton>(LoginScreenWidget->WidgetTree->FindWidget(TEXT("ConnectBtn")));

	if (!LoginLevelComboBox || !LoginConnectButton)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoginConn] Login screen widget must contain ComboBoxList and ConnectBtn widgets to enable manual connect"));
		return false;
	}

	if (!LoadServerTargetsFromLevelConfig())
	{
		UpdateConnectingStatus(TEXT("No dedicated server levels are configured in LevelConfig.json."));
		return true;
	}

	LoginLevelComboBox->ClearOptions();
	for (const FLoginServerTarget& ServerTarget : AvailableServerTargets)
	{
		LoginLevelComboBox->AddOption(ServerTarget.DisplayName);
	}

	LoginLevelComboBox->OnSelectionChanged.RemoveAll(this);
	LoginLevelComboBox->OnSelectionChanged.AddDynamic(this, &ALoginPlayerController::HandleLevelSelectionChanged);

	LoginConnectButton->OnClicked.RemoveAll(this);
	LoginConnectButton->OnClicked.AddDynamic(this, &ALoginPlayerController::HandleConnectButtonClicked);

	if (!AvailableServerTargets.IsEmpty())
	{
		LoginLevelComboBox->SetSelectedOption(AvailableServerTargets[0].DisplayName);
		ApplySelectedServerTarget(AvailableServerTargets[0].DisplayName);
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Login menu bound for manual connect with %d configured levels"), AvailableServerTargets.Num());
	return true;
}

bool ALoginPlayerController::LoadServerTargetsFromLevelConfig()
{
	AvailableServerTargets.Reset();

	const FString ConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LevelConfig.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Failed to read level config file: %s"), *ConfigPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Failed to parse level config JSON: %s"), *ConfigPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("levels"), LevelsArray) || LevelsArray == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Level config JSON does not contain a levels array: %s"), *ConfigPath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& LevelValue : *LevelsArray)
	{
		const TSharedPtr<FJsonObject>* LevelObject = nullptr;
		if (!LevelValue.IsValid() || !LevelValue->TryGetObject(LevelObject) || LevelObject == nullptr || !LevelObject->IsValid())
		{
			continue;
		}

		FLoginServerTarget ServerTarget;
		if (!(*LevelObject)->TryGetStringField(TEXT("displayName"), ServerTarget.DisplayName) || ServerTarget.DisplayName.IsEmpty())
		{
			continue;
		}

		(*LevelObject)->TryGetStringField(TEXT("mapPath"), ServerTarget.MapPath);

		double PortValue = 0.0;
		if ((*LevelObject)->TryGetNumberField(TEXT("port"), PortValue))
		{
			ServerTarget.ServerPort = static_cast<int32>(PortValue);
		}

		double QueryPortValue = 0.0;
		if ((*LevelObject)->TryGetNumberField(TEXT("queryPort"), QueryPortValue))
		{
			ServerTarget.QueryPort = static_cast<int32>(QueryPortValue);
		}

		if (ServerTarget.ServerPort >= 1 && ServerTarget.ServerPort <= 65535)
		{
			AvailableServerTargets.Add(ServerTarget);
		}
	}

	return !AvailableServerTargets.IsEmpty();
}

void ALoginPlayerController::ApplySelectedServerTarget(const FString& SelectedDisplayName)
{
	for (const FLoginServerTarget& ServerTarget : AvailableServerTargets)
	{
		if (ServerTarget.DisplayName.Equals(SelectedDisplayName, ESearchCase::CaseSensitive))
		{
			ServerPort = ServerTarget.ServerPort;
			UpdateConnectingStatus(FString::Printf(TEXT("Selected server: %s (%s)"), *ServerTarget.DisplayName, *BuildServerEndpoint()));
			UE_LOG(LogTemp, Display, TEXT("[LoginConn] Selected server target: %s -> %s"), *ServerTarget.DisplayName, *BuildServerEndpoint());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] Selected server target not found: %s"), *SelectedDisplayName);
}

void ALoginPlayerController::HandleConnectButtonClicked()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!LoginLevelComboBox || LoginLevelComboBox->GetSelectedOption().IsEmpty())
	{
		EnsureConnectingWidget();
		UpdateConnectingStatus(TEXT("Select a level before connecting."));
		return;
	}

	bConnectionAttempted = true;
	EnsureConnectingWidget();
	ExecuteClientConnect();
}

void ALoginPlayerController::HandleLevelSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	ApplySelectedServerTarget(SelectedItem);
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
		FString RequestedPlayerName;
		if (UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance()))
		{
			if (!AuraGameInstance->LoadSlotName.IsEmpty() &&
				UGameplayStatics::DoesSaveGameExist(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
			{
				if (USaveGame* SaveObject = UGameplayStatics::LoadGameFromSlot(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
				{
					if (const ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveObject))
					{
						RequestedPlayerName = LoadScreenSaveGame->PlayerName;
					}
				}
			}
		}

		if (RequestedPlayerName.IsEmpty())
		{
			RequestedPlayerName = FPlatformProcess::UserName(false);
		}

		if (RequestedPlayerName.IsEmpty())
		{
			RequestedPlayerName = TEXT("Player");
		}

		RequestedPlayerName.TrimStartAndEndInline();
		RequestedPlayerName.ReplaceInline(TEXT("?"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("&"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("="), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("#"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT(" "), TEXT("_"));

		const FString Command = FString::Printf(TEXT("open %s?PlayerName=%s"), *ServerEndpoint, *RequestedPlayerName);

		UE_LOG(LogTemp, Display, TEXT("[LoginConn] ExecuteClientConnect: command=%s"), *Command);

		bWaitingForConnectionResponse = true;

		ConsoleCommand(*Command);

		if (UWorld* World = GetWorld())
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			TimerManager.ClearTimer(ConnectionResponseWarningTimerHandle);
			TimerManager.ClearTimer(ConnectionTimeoutTimerHandle);

			TimerManager.SetTimer(
				ConnectionResponseWarningTimerHandle,
				this,
				&ALoginPlayerController::HandleConnectionResponseWarning,
				ConnectionResponseWarningDelay,
				false
			);

			TimerManager.SetTimer(
				ConnectionTimeoutTimerHandle,
				this,
				&ALoginPlayerController::HandleConnectionTimeout,
				ConnectionTimeoutDelay,
				false
			);

			UE_LOG(LogTemp, Display, TEXT("[LoginConn] ExecuteClientConnect: timers set (warning=%.2fs timeout=%.2fs)"),
				ConnectionResponseWarningDelay,
				ConnectionTimeoutDelay);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LoginConn] ExecuteClientConnect: world is null, timeout/warning timers not set"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConn] ExecuteClientConnect: skipped because controller is not local"));
	}
}

bool ALoginPlayerController::LoadServerConnectionFromJson()
{
	TArray<FString> CandidatePaths;
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), ConnectionConfigFileName));
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectConfigDir(), ConnectionConfigFileName));

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

	UE_LOG(LogTemp, Display, TEXT("[LoginConn] Loaded server connection config from %s -> Address=%s (port from LevelConfig selection)"),
		*LoadedFromPath,
		*ServerAddress);

	return true;
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

void ALoginPlayerController::HandleConnectionResponseWarning()
{
	if (!IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	const FString ServerEndpoint = BuildServerEndpoint();
	if (ServerEndpoint.IsEmpty())
	{
		UpdateConnectingStatus(TEXT("Still connecting... This is taking longer than usual."));
	}
	else
	{
		UpdateConnectingStatus(FString::Printf(TEXT("Still connecting to %s... This is taking longer than usual."), *ServerEndpoint));
	}
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] warning threshold reached after %.2f seconds"), ConnectionResponseWarningDelay);
}

void ALoginPlayerController::HandleConnectionTimeout()
{
	if (!IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	bWaitingForConnectionResponse = false;
	UpdateConnectingStatus(TEXT("Could not connect to server. Please check server status and try again."));
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] timeout reached after %.2f seconds"), ConnectionTimeoutDelay);
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

void ALoginPlayerController::BindConnectionFailureDelegates()
{
	if (bFailureDelegatesBound || !GEngine)
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] BindConnectionFailureDelegates skipped (AlreadyBound=%d GEngine=%d)"), bFailureDelegatesBound ? 1 : 0, GEngine ? 1 : 0);
		return;
	}

	GEngine->OnTravelFailure().AddUObject(this, &ALoginPlayerController::HandleTravelFailure);
	GEngine->OnNetworkFailure().AddUObject(this, &ALoginPlayerController::HandleNetworkFailure);
	bFailureDelegatesBound = true;
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] failure delegates bound"));
}

void ALoginPlayerController::UnbindConnectionFailureDelegates()
{
	if (!bFailureDelegatesBound || !GEngine)
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] UnbindConnectionFailureDelegates skipped (WasBound=%d GEngine=%d)"), bFailureDelegatesBound ? 1 : 0, GEngine ? 1 : 0);
		return;
	}

	GEngine->OnTravelFailure().RemoveAll(this);
	GEngine->OnNetworkFailure().RemoveAll(this);
	bFailureDelegatesBound = false;
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] failure delegates unbound"));
}

void ALoginPlayerController::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] HandleTravelFailure fired: code=%d waiting=%d world=%s error=%s"),
		static_cast<int32>(FailureType),
		bWaitingForConnectionResponse ? 1 : 0,
		*GetNameSafe(InWorld),
		*ErrorString);

	if (!IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] HandleTravelFailure ignored (Local=%d waiting=%d)"), IsLocalPlayerController() ? 1 : 0, bWaitingForConnectionResponse ? 1 : 0);
		return;
	}

	bWaitingForConnectionResponse = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectionResponseWarningTimerHandle);
		World->GetTimerManager().ClearTimer(ConnectionTimeoutTimerHandle);
	}

	const FString FailureCode = FString::FromInt(static_cast<int32>(FailureType));

	const FString Message = FString::Printf(
		TEXT("Connection failed (code %s). %s"),
		*FailureCode,
		ErrorString.IsEmpty() ? TEXT("Please try again.") : *ErrorString);

	UpdateConnectingStatus(Message);
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] travel failure handled: code=%s | %s"), *FailureCode, *ErrorString);
}

void ALoginPlayerController::HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] HandleNetworkFailure fired: code=%d waiting=%d world=%s netDriver=%s error=%s"),
		static_cast<int32>(FailureType),
		bWaitingForConnectionResponse ? 1 : 0,
		*GetNameSafe(InWorld),
		*GetNameSafe(NetDriver),
		*ErrorString);

	if (!IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] HandleNetworkFailure ignored (Local=%d waiting=%d)"), IsLocalPlayerController() ? 1 : 0, bWaitingForConnectionResponse ? 1 : 0);
		return;
	}

	bWaitingForConnectionResponse = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectionResponseWarningTimerHandle);
		World->GetTimerManager().ClearTimer(ConnectionTimeoutTimerHandle);
	}

	const FString FailureCode = FString::FromInt(static_cast<int32>(FailureType));

	const FString Message = FString::Printf(
		TEXT("Network error (code %s). %s"),
		*FailureCode,
		ErrorString.IsEmpty() ? TEXT("Please check your network and try again.") : *ErrorString);

	UpdateConnectingStatus(Message);
	UE_LOG(LogTemp, Warning, TEXT("[LoginConn] network failure handled: code=%s | %s"), *FailureCode, *ErrorString);
}
