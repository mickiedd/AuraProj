// Copyright Druid Mechanics

#include "Client/AuraClientDisconnectHandler.h"
#include "Game/AuraGameInstance.h"
#include "Game/ServerTravelComponent.h"
#include "Network/AuraHeartbeatComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Aura/AuraLogChannels.h"

UAuraClientDisconnectHandler::UAuraClientDisconnectHandler()
{
	PrimaryComponentTick.bCanEverTick = false;

	LostMessage = FText::FromString(TEXT("Connection to the server has been lost."));
}

void UAuraClientDisconnectHandler::BeginPlay()
{
	Super::BeginPlay();

	// Only pure clients experience "lost the dedicated server mid-game".
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	BindFailureDelegates();

	// Detector 2: bind to the owning controller's heartbeat component, if present.
	if (UAuraHeartbeatComponent* HeartbeatComp = PC->FindComponentByClass<UAuraHeartbeatComponent>())
	{
		BindHeartbeat(HeartbeatComp);
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("[ServerLost] No UAuraHeartbeatComponent on %s; heartbeat detector disabled."), *GetNameSafe(PC));
	}

	// Sample the server connection state until we observe USOCK_Open, then stop and arm the movement watchdog.
	World->GetTimerManager().SetTimer(
		ConnectionPollHandle,
		this,
		&UAuraClientDisconnectHandler::PollConnectionState,
		FMath::Max(0.1f, ConnectionStatePollInterval),
		true);

	UE_LOG(LogAura, Log, TEXT("[ServerLost] Handler active on client (PC=%s)."), *GetNameSafe(PC));
}

void UAuraClientDisconnectHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	UnbindFailureDelegates();
	Super::EndPlay(EndPlayReason);
}

void UAuraClientDisconnectHandler::PollConnectionState()
{
	if (bWasConnected)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	if (UNetConnection* Conn = PC->GetNetConnection())
	{
		if (Conn->GetConnectionState() == USOCK_Open)
		{
			bWasConnected = true;
			// We are in the game; stop polling. Future loss is reported via the
			// heartbeat/movement-watchdog/OnNetworkFailure detectors.
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(ConnectionPollHandle);

				// Detector 3: arm the movement-stall watchdog now that we are connected.
				World->GetTimerManager().SetTimer(
					MovementStallHandle,
					this,
					&UAuraClientDisconnectHandler::PollMovementStall,
					FMath::Max(0.1f, MovementStallPollInterval),
					true);
			}
			UE_LOG(LogAura, Log, TEXT("[ServerLost] Client connection reached USOCK_Open; loss detection armed (heartbeat + movement watchdog)."));
		}
	}
}

void UAuraClientDisconnectHandler::HandleNetworkFailure(UWorld* InWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 1. Act once — ConnectionLost fires every tick once the connection is closed.
	if (bHandled)
	{
		return;
	}

	// 2. Only mid-game loss; the connect window is owned by UServerTravelComponent.
	if (!bWasConnected)
	{
		return;
	}

	// 3. A legitimate travel is in flight -> the engine is tearing the connection down on purpose.
	//    (Engine already suppresses ConnectionLost while PendingNetGame exists; this is a safety net.)
	if (GEngine && InWorld && GEngine->PendingNetGameFromWorld(InWorld) != nullptr)
	{
		return;
	}

	// 4. Only treat "the server connection is gone" as a loss.
	if (!IsLossFailure(FailureType))
	{
		return;
	}

	UE_LOG(LogAura, Warning, TEXT("[ServerLost] Mid-game network failure detected: type=%d error='%s'"),
		static_cast<int32>(FailureType), *ErrorString);

	OnServerLost(ErrorString);
}

void UAuraClientDisconnectHandler::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	// Travel failures during the connect window are surfaced by UServerTravelComponent.
	// We only care about a travel failure that happens after we were connected (rare).
	if (bHandled || !bWasConnected)
	{
		return;
	}

	UE_LOG(LogAura, Warning, TEXT("[ServerLost] Mid-game travel failure: type=%d error='%s'"),
		static_cast<int32>(FailureType), *ErrorString);

	OnServerLost(ErrorString);
}

bool UAuraClientDisconnectHandler::IsLossFailure(ENetworkFailure::Type FailureType)
{
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::PendingConnectionFailure:
		return true;
	default:
		return false;
	}
}

void UAuraClientDisconnectHandler::OnServerLost(const FString& ErrorString)
{
	if (bHandled)
	{
		return;
	}
	bHandled = true;

	// Stash the message for the Login screen, then travel immediately.
	if (UAuraGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UAuraGameInstance>() : nullptr)
	{
		GI->PendingServerLostMessage = LostMessage.ToString();
	}

	ReturnToLogin();
}

void UAuraClientDisconnectHandler::HandleHeartbeatLost()
{
	if (bHandled || !bWasConnected)
	{
		return;
	}

	UE_LOG(LogAura, Warning, TEXT("[ServerLost] Heartbeat lost: server stopped answering pings."));
	OnServerLost(TEXT("Heartbeat lost"));
}

void UAuraClientDisconnectHandler::PollMovementStall()
{
	if (bHandled || !bWasConnected)
	{
		return;
	}

	// Only meaningful while the client is actively producing moves; an idle client
	// won't fill SavedMoves and relies on the heartbeat instead.
	UCharacterMovementComponent* MoveComp = GetOwnedCharacterMovement();
	if (!MoveComp)
	{
		StallSinceRealtime = 0.0;
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = MoveComp->GetPredictionData_Client_Character();
	const int32 SavedMoveCount = ClientData ? ClientData->SavedMoves.Num() : 0;

	// If we can't read the prediction data yet, don't false-trigger; just wait.
	if (!ClientData)
	{
		return;
	}

	if (SavedMoveCount >= FMath::Max(1, MovementStallSavedMoveThreshold))
	{
		if (StallSinceRealtime == 0.0)
		{
			StallSinceRealtime = FPlatformTime::Seconds();
		}
		else if ((FPlatformTime::Seconds() - StallSinceRealtime) >= MovementStallDuration)
		{
			UE_LOG(LogAura, Warning,
				TEXT("[ServerLost] Movement stalled: SavedMoves=%d saturated for %.2fs (threshold=%d, duration=%.1fs)."),
				SavedMoveCount, FPlatformTime::Seconds() - StallSinceRealtime,
				MovementStallSavedMoveThreshold, MovementStallDuration);
			OnServerLost(TEXT("Server stopped responding to movement"));
		}
	}
	else
	{
		// Buffer drained (server acked) — reset the stall window.
		StallSinceRealtime = 0.0;
	}
}

UCharacterMovementComponent* UAuraClientDisconnectHandler::GetOwnedCharacterMovement() const
{
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC))
	{
		return nullptr;
	}

	if (APawn* Pawn = PC->GetPawn())
	{
		return Cast<UCharacterMovementComponent>(Pawn->FindComponentByClass<UCharacterMovementComponent>());
	}
	return nullptr;
}

void UAuraClientDisconnectHandler::BindHeartbeat(UAuraHeartbeatComponent* HeartbeatComp)
{
	if (!HeartbeatComp || bHeartbeatBound)
	{
		return;
	}

	HeartbeatLostHandle = HeartbeatComp->OnHeartbeatLost.AddUObject(this, &UAuraClientDisconnectHandler::HandleHeartbeatLost);
	bHeartbeatBound = true;
}

void UAuraClientDisconnectHandler::ReturnToLogin()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC))
	{
		UE_LOG(LogAura, Warning, TEXT("[ServerLost] ReturnToLogin: owning PlayerController is invalid; cannot travel."));
		return;
	}

	UE_LOG(LogAura, Log, TEXT("[ServerLost] Traveling back to Login level (%s)."), *UServerTravelComponent::LoginLevelPath);
	PC->ClientTravel(UServerTravelComponent::LoginLevelPath, TRAVEL_Absolute);
}

void UAuraClientDisconnectHandler::BindFailureDelegates()
{
	if (bFailureDelegatesBound || !GEngine)
	{
		return;
	}

	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UAuraClientDisconnectHandler::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UAuraClientDisconnectHandler::HandleTravelFailure);
	bFailureDelegatesBound = true;
}

void UAuraClientDisconnectHandler::UnbindFailureDelegates()
{
	if (bFailureDelegatesBound && GEngine)
	{
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
			NetworkFailureHandle.Reset();
		}
		if (TravelFailureHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureHandle);
			TravelFailureHandle.Reset();
		}
		bFailureDelegatesBound = false;
	}

	if (bHeartbeatBound)
	{
		if (APlayerController* PC = GetOwningPlayerController())
		{
			if (UAuraHeartbeatComponent* HeartbeatComp = PC->FindComponentByClass<UAuraHeartbeatComponent>())
			{
				if (HeartbeatLostHandle.IsValid())
				{
					HeartbeatComp->OnHeartbeatLost.Remove(HeartbeatLostHandle);
					HeartbeatLostHandle.Reset();
				}
			}
		}
		bHeartbeatBound = false;
	}
}

void UAuraClientDisconnectHandler::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectionPollHandle);
		World->GetTimerManager().ClearTimer(MovementStallHandle);
	}
	StallSinceRealtime = 0.0;
}

APlayerController* UAuraClientDisconnectHandler::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}