// Copyright Druid Mechanics

#include "Game/ServerTravelComponent.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UServerTravelComponent::UServerTravelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UServerTravelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMonitoringTimers();
	bWaitingForConnectionResponse = false;
	UnbindConnectionFailureDelegates();
	Super::EndPlay(EndPlayReason);
}

UServerTravelComponent* UServerTravelComponent::GetOrCreateFor(APlayerController* InPlayerController)
{
	if (!IsValid(InPlayerController))
	{
		return nullptr;
	}

	if (UServerTravelComponent* Existing = InPlayerController->FindComponentByClass<UServerTravelComponent>())
	{
		return Existing;
	}

	UServerTravelComponent* Created = NewObject<UServerTravelComponent>(InPlayerController, TEXT("ServerTravelComponentRuntime"));
	if (!IsValid(Created))
	{
		return nullptr;
	}

	InPlayerController->AddOwnedComponent(Created);
	Created->RegisterComponent();
	return Created;
}

void UServerTravelComponent::ConfigureLocalTravelMonitoring(float InWarningDelaySeconds, float InTimeoutDelaySeconds)
{
	ConnectionResponseWarningDelay = FMath::Max(1.0f, InWarningDelaySeconds);
	ConnectionTimeoutDelay = FMath::Max(ConnectionResponseWarningDelay + 0.1f, InTimeoutDelaySeconds);
}

bool UServerTravelComponent::TravelToServer(const FString& ServerEndpoint, const FString& RequestedPlayerName)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerTravelComponent] TravelToServer failed: owning player controller is invalid"));
		return false;
	}

	FString TrimmedEndpoint = ServerEndpoint;
	TrimmedEndpoint.TrimStartAndEndInline();
	if (TrimmedEndpoint.IsEmpty())
	{
		BroadcastStatusMessage(TEXT("Server configuration is invalid. Please check server endpoint settings."));
		UE_LOG(LogTemp, Error, TEXT("[ServerTravelComponent] TravelToServer failed: endpoint is empty"));
		return false;
	}

	const FString TravelUrl = BuildTravelUrl(TrimmedEndpoint, RequestedPlayerName);
	UE_LOG(LogTemp, Display, TEXT("[ServerTravelComponent] Executing travel for PC=%s url=%s local=%d"),
		*GetNameSafe(PlayerController),
		*TravelUrl,
		PlayerController->IsLocalPlayerController() ? 1 : 0);

	if (PlayerController->IsLocalPlayerController())
	{
		ActiveServerEndpoint = TrimmedEndpoint;
		bWaitingForConnectionResponse = true;
		BindConnectionFailureDelegates();
		ClearMonitoringTimers();

		if (UWorld* World = GetWorld())
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			TimerManager.SetTimer(ConnectionResponseWarningTimerHandle, this, &UServerTravelComponent::HandleConnectionResponseWarning, ConnectionResponseWarningDelay, false);
			TimerManager.SetTimer(ConnectionTimeoutTimerHandle, this, &UServerTravelComponent::HandleConnectionTimeout, ConnectionTimeoutDelay, false);
		}
	}

	PlayerController->ClientTravel(TravelUrl, TRAVEL_Absolute);
	return true;
}

void UServerTravelComponent::HandleConnectionResponseWarning()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	if (ActiveServerEndpoint.IsEmpty())
	{
		BroadcastStatusMessage(TEXT("Still connecting... This is taking longer than usual."));
	}
	else
	{
		BroadcastStatusMessage(FString::Printf(TEXT("Still connecting to %s... This is taking longer than usual."), *ActiveServerEndpoint));
	}
}

void UServerTravelComponent::HandleConnectionTimeout()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	bWaitingForConnectionResponse = false;
	ClearMonitoringTimers();
	BroadcastStatusMessage(TEXT("Could not connect to server. Please check server status and try again."));
}

void UServerTravelComponent::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	(void)InWorld;

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	bWaitingForConnectionResponse = false;
	ClearMonitoringTimers();

	const FString FailureCode = FString::FromInt(static_cast<int32>(FailureType));
	BroadcastStatusMessage(FString::Printf(TEXT("Connection failed (code %s). %s"),
		*FailureCode,
		ErrorString.IsEmpty() ? TEXT("Please try again.") : *ErrorString));
}

void UServerTravelComponent::HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	(void)InWorld;
	(void)NetDriver;

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalPlayerController() || !bWaitingForConnectionResponse)
	{
		return;
	}

	bWaitingForConnectionResponse = false;
	ClearMonitoringTimers();

	const FString FailureCode = FString::FromInt(static_cast<int32>(FailureType));
	BroadcastStatusMessage(FString::Printf(TEXT("Network error (code %s). %s"),
		*FailureCode,
		ErrorString.IsEmpty() ? TEXT("Please check your network and try again.") : *ErrorString));
}

void UServerTravelComponent::BindConnectionFailureDelegates()
{
	if (bFailureDelegatesBound || !GEngine)
	{
		return;
	}

	GEngine->OnTravelFailure().AddUObject(this, &UServerTravelComponent::HandleTravelFailure);
	GEngine->OnNetworkFailure().AddUObject(this, &UServerTravelComponent::HandleNetworkFailure);
	bFailureDelegatesBound = true;
}

void UServerTravelComponent::UnbindConnectionFailureDelegates()
{
	if (!bFailureDelegatesBound || !GEngine)
	{
		return;
	}

	GEngine->OnTravelFailure().RemoveAll(this);
	GEngine->OnNetworkFailure().RemoveAll(this);
	bFailureDelegatesBound = false;
}

void UServerTravelComponent::ClearMonitoringTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(ConnectionResponseWarningTimerHandle);
		TimerManager.ClearTimer(ConnectionTimeoutTimerHandle);
	}
}

void UServerTravelComponent::BroadcastStatusMessage(const FString& InMessage) const
{
	OnStatusMessage.Broadcast(InMessage);

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!OnStatusMessage.IsBound() && GEngine && IsValid(PlayerController) && PlayerController->IsLocalPlayerController())
	{
		const uint64 MessageKey = static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
		GEngine->AddOnScreenDebugMessage(MessageKey, 6.0f, FColor::Yellow, InMessage);
	}
}

FString UServerTravelComponent::BuildTravelUrl(const FString& ServerEndpoint, const FString& RequestedPlayerName) const
{
	const FString SanitizedPlayerName = SanitizePlayerName(RequestedPlayerName);
	if (SanitizedPlayerName.IsEmpty())
	{
		return ServerEndpoint;
	}

	const TCHAR Delimiter = ServerEndpoint.Contains(TEXT("?")) ? TEXT('&') : TEXT('?');
	return FString::Printf(TEXT("%s%cPlayerName=%s"), *ServerEndpoint, Delimiter, *SanitizedPlayerName);
}

FString UServerTravelComponent::SanitizePlayerName(FString PlayerName)
{
	PlayerName.TrimStartAndEndInline();
	PlayerName.ReplaceInline(TEXT("?"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("&"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("="), TEXT("_"));
	PlayerName.ReplaceInline(TEXT("#"), TEXT("_"));
	PlayerName.ReplaceInline(TEXT(" "), TEXT("_"));
	return PlayerName;
}

APlayerController* UServerTravelComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}
