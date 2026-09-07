// Copyright Druid Mechanics

#include "Game/ServerTravelComponent.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Game/AuraGameInstance.h"

const FString UServerTravelComponent::LoadingLevelPath = TEXT("/Game/Maps/Loading");
const FString UServerTravelComponent::LoginLevelPath   = TEXT("/Game/Maps/Login");

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

bool UServerTravelComponent::TravelToServer(const FString& ServerEndpoint, const FString& RequestedPlayerName, FName RequestedRoleId)
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

	const FString TravelUrl = BuildTravelUrl(TrimmedEndpoint, RequestedPlayerName, RequestedRoleId);
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
	const bool bVersionMismatch =
		ErrorString.Contains(TEXT("incompatible version"), ESearchCase::IgnoreCase) ||
		ErrorString.Contains(TEXT("RemoteNetworkVersion"), ESearchCase::IgnoreCase) ||
		ErrorString.Contains(TEXT("LocalNetworkVersion"), ESearchCase::IgnoreCase);
	const FString UserMessage = bVersionMismatch
		? TEXT("Client/server build mismatch. Rebuild and restart UnrealEditor from the same project and engine build as the server, then retry.")
		: (ErrorString.IsEmpty() ? TEXT("Please check your network and try again.") : *ErrorString);
	if (UAuraGameInstance* GI = Cast<UAuraGameInstance>(PlayerController->GetGameInstance()))
	{
		GI->PendingServerLostMessage = UserMessage;
	}
	BroadcastStatusMessage(FString::Printf(TEXT("Network error (code %s). %s"),
		*FailureCode,
		*UserMessage));
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

FString UServerTravelComponent::BuildTravelUrl(const FString& ServerEndpoint, const FString& RequestedPlayerName, FName RequestedRoleId) const
{
	const FString SanitizedPlayerName = SanitizePlayerName(RequestedPlayerName);
	FString Url = ServerEndpoint;
	if (!SanitizedPlayerName.IsEmpty()) Url += FString::Printf(TEXT("?PlayerName=%s"), *SanitizedPlayerName);
	if (!RequestedRoleId.IsNone()) Url += FString::Printf(TEXT("?Role=%s"), *RequestedRoleId.ToString());
	return Url;
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

bool UServerTravelComponent::TravelToServerViaLoadingLevel(const FString& ServerEndpoint, const FString& RequestedPlayerName, FName RequestedRoleId)
{
	RouteToServerViaLoadingLevel(GetOwningPlayerController(), ServerEndpoint, RequestedPlayerName, RequestedRoleId);
	return IsValid(GetOwningPlayerController());
}

void UServerTravelComponent::RouteToServerViaLoadingLevel(APlayerController* InPC, const FString& ServerEndpoint, const FString& RequestedPlayerName, FName RequestedRoleId)
{
	if (!IsValid(InPC))
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerTravelComponent] RouteToServerViaLoadingLevel: PlayerController is invalid"));
		return;
	}

	FString TrimmedEndpoint = ServerEndpoint;
	TrimmedEndpoint.TrimStartAndEndInline();
	if (TrimmedEndpoint.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerTravelComponent] RouteToServerViaLoadingLevel: endpoint is empty"));
		return;
	}

	const FString SafePlayerName = SanitizePlayerName(RequestedPlayerName);

	// Encode destination into Loading level URL options.
	// ':' is safe inside Unreal URL options (it is not a URL separator).
	FString LoadingUrl = FString::Printf(TEXT("%s?Dest=%s"), *LoadingLevelPath, *TrimmedEndpoint);
	if (!SafePlayerName.IsEmpty())
	{
		LoadingUrl += FString::Printf(TEXT("?PName=%s"), *SafePlayerName);
	}
	if (!RequestedRoleId.IsNone())
	{
		LoadingUrl += FString::Printf(TEXT("?Role=%s"), *RequestedRoleId.ToString());
	}

	UE_LOG(LogTemp, Display, TEXT("[ServerTravelComponent] Routing PC=%s to loading level -> final dest=%s"),
		*GetNameSafe(InPC), *TrimmedEndpoint);

	InPC->ClientTravel(LoadingUrl, TRAVEL_Absolute);
}

void UServerTravelComponent::RouteToMapViaLoadingLevel(UObject* WorldContextObject, const FString& MapAssetName)
{
	if (!IsValid(WorldContextObject) || MapAssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerTravelComponent] RouteToMapViaLoadingLevel: invalid context or empty map name"));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	if (!IsValid(World)) return;

	if (UAuraGameInstance* GI = World->GetGameInstance<UAuraGameInstance>())
	{
		GI->PendingMapAssetName = MapAssetName;
		GI->PendingMapSoftPtr.Reset();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerTravelComponent] RouteToMapViaLoadingLevel: UAuraGameInstance not found; will travel to Loading but destination may be lost"));
	}

	UE_LOG(LogTemp, Display, TEXT("[ServerTravelComponent] RouteToMapViaLoadingLevel: map=%s via loading level"), *MapAssetName);
	UGameplayStatics::OpenLevel(WorldContextObject, FName(*LoadingLevelPath));
}

void UServerTravelComponent::RouteToMapBySoftPtrViaLoadingLevel(UObject* WorldContextObject, const TSoftObjectPtr<UWorld>& SoftMapPtr)
{
	if (!IsValid(WorldContextObject) || SoftMapPtr.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerTravelComponent] RouteToMapBySoftPtrViaLoadingLevel: invalid context or null soft map"));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	if (!IsValid(World)) return;

	if (UAuraGameInstance* GI = World->GetGameInstance<UAuraGameInstance>())
	{
		GI->PendingMapSoftPtr = SoftMapPtr;
		GI->PendingMapAssetName.Empty();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerTravelComponent] RouteToMapBySoftPtrViaLoadingLevel: UAuraGameInstance not found; will travel to Loading but destination may be lost"));
	}

	UE_LOG(LogTemp, Display, TEXT("[ServerTravelComponent] RouteToMapBySoftPtrViaLoadingLevel: map=%s via loading level"), *SoftMapPtr.ToString());
	UGameplayStatics::OpenLevel(WorldContextObject, FName(*LoadingLevelPath));
}
