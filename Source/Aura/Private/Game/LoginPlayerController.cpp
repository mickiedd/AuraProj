// Copyright Druid Mechanics

#include "Game/LoginPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Game/AuraGameInstance.h"
#include "Game/LoadScreenSaveGame.h"
#include "Game/LoginGameMode.h"
#include "UI/Widget/LoginConnectingWidget.h"

void ALoginPlayerController::BeginPlay()
{
	Super::BeginPlay();
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
	}

	// Only execute on client (IsLocalPlayerController returns true only for local clients)
	if (IsLocalPlayerController() && bAutoConnectToServer && !bConnectionAttempted)
	{
		bConnectionAttempted = true;
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: auto-connect flow entered"));

		// Create the connecting widget if class is set
		if (ConnectingWidgetClass)
		{
			ConnectingWidget = CreateWidget<ULoginConnectingWidget>(this, ConnectingWidgetClass);
			if (ConnectingWidget)
			{
				ConnectingWidget->AddToViewport(1);
				ConnectingWidget->ShowConnecting(TEXT("Connecting to server..."));
				UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: connecting widget created and shown: %s"), *GetNameSafe(ConnectingWidget));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoginConn] BeginPlay: failed to create connecting widget from class %s"), *GetNameSafe(ConnectingWidgetClass));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoginConn] BeginPlay: ConnectingWidgetClass is null, no UI status will be shown"));
		}

		// Add a small delay to ensure UI and networking are fully initialized
		if (UWorld* World = GetWorld())
		{
			UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: scheduling ExecuteClientConnect in 0.50s"));
			World->GetTimerManager().SetTimer(
				ConnectionTimerHandle,
				this,
				&ALoginPlayerController::ExecuteClientConnect,
				0.5f,
				false
			);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LoginConn] BeginPlay: world is null, cannot schedule connection"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] BeginPlay: auto-connect skipped (Local=%d Auto=%d Attempted=%d)"),
			IsLocalPlayerController() ? 1 : 0,
			bAutoConnectToServer ? 1 : 0,
			bConnectionAttempted ? 1 : 0);
	}
}

void ALoginPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: Pawn=%s Local=%d AutoConnect=%d Attempted=%d"),
		*GetNameSafe(InPawn),
		IsLocalPlayerController() ? 1 : 0,
		bAutoConnectToServer ? 1 : 0,
		bConnectionAttempted ? 1 : 0);

	// Attempt connection when possessed (in case BeginPlay didn't trigger)
	if (IsLocalPlayerController() && bAutoConnectToServer && !bConnectionAttempted)
	{
		bConnectionAttempted = true;
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: auto-connect flow entered"));

		// Create the connecting widget if class is set
		if (ConnectingWidgetClass)
		{
			ConnectingWidget = CreateWidget<ULoginConnectingWidget>(this, ConnectingWidgetClass);
			if (ConnectingWidget)
			{
				ConnectingWidget->AddToViewport(1);
				ConnectingWidget->ShowConnecting(TEXT("Connecting to server..."));
				UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: connecting widget created and shown: %s"), *GetNameSafe(ConnectingWidget));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoginConn] OnPossess: failed to create connecting widget from class %s"), *GetNameSafe(ConnectingWidgetClass));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoginConn] OnPossess: ConnectingWidgetClass is null, no UI status will be shown"));
		}

		if (UWorld* World = GetWorld())
		{
			UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: scheduling ExecuteClientConnect in 0.50s"));
			World->GetTimerManager().SetTimer(
				ConnectionTimerHandle,
				this,
				&ALoginPlayerController::ExecuteClientConnect,
				0.5f,
				false
			);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[LoginConn] OnPossess: world is null, cannot schedule connection"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[LoginConn] OnPossess: auto-connect skipped (Local=%d Auto=%d Attempted=%d)"),
			IsLocalPlayerController() ? 1 : 0,
			bAutoConnectToServer ? 1 : 0,
			bConnectionAttempted ? 1 : 0);
	}
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

	Super::EndPlay(EndPlayReason);
}

void ALoginPlayerController::ExecuteClientConnect()
{
	UE_LOG(LogTemp, Display, TEXT("[LoginConn] ExecuteClientConnect: Local=%d ServerAddress=%s"),
		IsLocalPlayerController() ? 1 : 0,
		*ServerAddress);

	if (IsLocalPlayerController())
	{
		UpdateConnectingStatus(TEXT("Connecting to server..."));

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

		const FString Command = FString::Printf(TEXT("open %s?PlayerName=%s"), *ServerAddress, *RequestedPlayerName);

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

void ALoginPlayerController::HandleConnectionResponseWarning()
{
	if (!IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	UpdateConnectingStatus(TEXT("Still connecting... This is taking longer than usual."));
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
