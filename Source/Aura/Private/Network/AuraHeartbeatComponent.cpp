// Copyright Druid Mechanics

#include "Network/AuraHeartbeatComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Aura/AuraLogChannels.h"

UAuraHeartbeatComponent::UAuraHeartbeatComponent()
{
	SetIsReplicatedByDefault(true);
	SetComponentTickEnabled(false);
}

bool UAuraHeartbeatComponent::IsOwningClient() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	// Local controller on a pure client = the owning client's gameplay controller.
	return PC != nullptr && PC->IsLocalPlayerController() && GetOwnerRole() == ROLE_AutonomousProxy;
}

void UAuraHeartbeatComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client || !IsOwningClient())
	{
		return;
	}

	// Arm LastPongRealtime to "now" so we don't false-trigger during the initial connect/movement sync
	// before the first pong arrives; bArmed stays false until that first pong.
	LastPongRealtime = FPlatformTime::Seconds();

	World->GetTimerManager().SetTimer(SendHandle, this, &UAuraHeartbeatComponent::SendHeartbeat, FMath::Max(0.2f, HeartbeatInterval), true);
	World->GetTimerManager().SetTimer(CheckHandle, this, &UAuraHeartbeatComponent::CheckHeartbeat, FMath::Max(0.1f, WatchdogTickInterval), true);

	UE_LOG(LogAura, Log, TEXT("[Heartbeat] Active on owning client (interval=%.2fs timeout=%.2fs)."), HeartbeatInterval, HeartbeatTimeout);
}

void UAuraHeartbeatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	Super::EndPlay(EndPlayReason);
}

void UAuraHeartbeatComponent::SendHeartbeat()
{
	ServerHeartbeat(NextSequence++);
}

void UAuraHeartbeatComponent::ServerHeartbeat_Implementation(int32 Sequence)
{
	// Server authority: echo the ping back to the owning client.
	// (Only runs on the server-side instance of this component for the calling client.)
	ClientHeartbeatAck(Sequence);
}

void UAuraHeartbeatComponent::ClientHeartbeatAck_Implementation(int32 Sequence)
{
	LastPongRealtime = FPlatformTime::Seconds();
	if (!bArmed)
	{
		bArmed = true;
		UE_LOG(LogAura, Log, TEXT("[Heartbeat] Armed (first pong received, seq=%d)."), Sequence);
	}
}

void UAuraHeartbeatComponent::CheckHeartbeat()
{
	if (!bArmed)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastPongRealtime > HeartbeatTimeout)
	{
		UE_LOG(LogAura, Warning, TEXT("[Heartbeat] Lost: no pong for %.2fs (timeout=%.2fs). Declaring server lost."),
			Now - LastPongRealtime, HeartbeatTimeout);

		// Stop the watchdog so we only broadcast once; the disconnect handler tears us down via travel.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CheckHandle);
		}

		OnHeartbeatLost.Broadcast();
	}
}

void UAuraHeartbeatComponent::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SendHandle);
		World->GetTimerManager().ClearTimer(CheckHandle);
	}
}