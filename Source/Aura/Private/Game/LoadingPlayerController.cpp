// Copyright Druid Mechanics

#include "Game/LoadingPlayerController.h"
#include "Game/GameServerClient.h"
#include "Game/ServerTravelComponent.h"
#include "Game/AuraGameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "UI/WebUI/WebUIBridgeSubsystem.h"
#include "UI/WebUI/WebUIWidget.h"

ALoadingPlayerController::ALoadingPlayerController()
{
	ServerTravelComponent = CreateDefaultSubobject<UServerTravelComponent>(TEXT("ServerTravelComponent"));
	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] Constructor: ALoadingPlayerController created"));
}

void ALoadingPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: PC=%s Local=%d world=%s"),
		*GetName(), (int32)IsLocalPlayerController(), *GetNameSafe(GetWorld()));

	// Only the local client resolves a pending cross-server travel.
	if (!IsLocalPlayerController())
	{
		UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: not local controller, skipping Dest resolution"));
		return;
	}

	EnsureLoadingWidget();
	SetLoadingProgressTarget(12.f, TEXT("Loading screen ready..."));

	// Dump the raw URL options so we can verify they arrived correctly.
	FString OptionsString;
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: World->URL.Op count=%d"), World->URL.Op.Num());
		for (const FString& Op : World->URL.Op)
		{
			UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: URL option: '%s'"), *Op);
			OptionsString += TEXT("?") + Op;
		}
	}

	const FString Dest = UGameplayStatics::ParseOption(OptionsString, TEXT("Dest"));
	if (Dest.IsEmpty())
	{
		const FString PortalServerId = UGameplayStatics::ParseOption(OptionsString, TEXT("PortalServerId"));
		if (!PortalServerId.IsEmpty())
		{
			const FString PortalFallbackEndpoint = UGameplayStatics::ParseOption(OptionsString, TEXT("PortalFallback"));
			const FString PortalPlayerName = UGameplayStatics::ParseOption(OptionsString, TEXT("PName"));
			const FName PortalRoleId(*UGameplayStatics::ParseOption(OptionsString, TEXT("Role")));

			UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: portal GSM request detected serverId='%s' fallback='%s' player='%s'"),
				*PortalServerId, *PortalFallbackEndpoint, *PortalPlayerName);
			SetLoadingProgressTarget(28.f, FString::Printf(TEXT("Resolving dungeon %s..."), *PortalServerId));

			if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
			{
				const FString GSMAddress = GI->GameServerAddress.IsEmpty() ? TEXT("127.0.0.1") : GI->GameServerAddress;
				const int32 GSMPort = GI->GameServerPort > 0 ? GI->GameServerPort : 9000;

				// Keep the client object alive for the async GSM request.
				GameServerClient = NewObject<UGameServerClient>(this);
				if (!IsValid(GameServerClient))
				{
					UE_LOG(LogTemp, Error, TEXT("[LoadingPC] BeginPlay: failed to allocate GameServerClient for portal GSM request"));
					return;
				}

				UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: querying GSM at %s:%d for portal serverId='%s'"),
					*GSMAddress, GSMPort, *PortalServerId);
				constexpr float PortalGsmTimeoutSeconds = 20.0f;
				UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: portal GSM timeout set to %.1fs"), PortalGsmTimeoutSeconds);

				TWeakObjectPtr<ALoadingPlayerController> WeakThis(this);
				GameServerClient->RequestServer(
					GSMAddress,
					GSMPort,
					PortalServerId,
					PortalGsmTimeoutSeconds,
					FOnGameServerResponse::CreateLambda([WeakThis, PortalFallbackEndpoint, PortalPlayerName, PortalRoleId](const FGameServerResponse& Response)
					{
						if (ALoadingPlayerController* PC = WeakThis.Get())
						{
							if (Response.bSuccess)
							{
								const FString FinalEndpoint = FString::Printf(TEXT("%s:%d"), *Response.Host, Response.Port);
								UE_LOG(LogTemp, Display, TEXT("[LoadingPC] Portal GSM success -> endpoint=%s player='%s'"), *FinalEndpoint, *PortalPlayerName);
								PC->SetLoadingProgressTarget(88.f, FString::Printf(TEXT("Connecting to %s..."), *FinalEndpoint));
								if (IsValid(PC->ServerTravelComponent))
								{
									PC->ServerTravelComponent->TravelToServer(FinalEndpoint, PortalPlayerName, PortalRoleId);
								}
								return;
							}

							if (!PortalFallbackEndpoint.IsEmpty())
							{
								UE_LOG(LogTemp, Warning, TEXT("[LoadingPC] Portal GSM failed (%s); using fallback endpoint=%s"), *Response.ErrorMessage, *PortalFallbackEndpoint);
								PC->SetLoadingProgressTarget(88.f, FString::Printf(TEXT("Connecting to %s..."), *PortalFallbackEndpoint));
								if (IsValid(PC->ServerTravelComponent))
								{
									PC->ServerTravelComponent->TravelToServer(PortalFallbackEndpoint, PortalPlayerName, PortalRoleId);
								}
								return;
							}

							UE_LOG(LogTemp, Error, TEXT("[LoadingPC] Portal GSM failed with no fallback: %s"), *Response.ErrorMessage);
							PC->SetLoadingProgressTarget(100.f, TEXT("Connection failed. Returning to Login..."));
						}
					}));
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("[LoadingPC] BeginPlay: portal GSM option found but GameInstance is invalid"));
			return;
		}

		// No Dest URL option — check whether the Login-flow GSM bridge in GameInstance
		// has a pending cross-server travel (the LoginPlayerController traveled here before
		// the GSM query resolved and stored the player name in GI).
		if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
		{
			// The GSM callback caches its result before broadcasting, so it may have
			// already resolved by the time BeginPlay runs (the broadcast went to zero
			// listeners because we had not bound yet).  Consume the cached endpoint
			// directly instead of waiting on a broadcast that already happened.
			if (GI->HasResolvedCrossServerTravel())
			{
				const FString Endpoint = GI->PendingCrossServerResolvedEndpoint;
				const FString PlayerName = GI->PendingCrossServerResolvedPlayerName;
				const FName RoleId = GI->PendingCrossServerRoleId;
				UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: login-flow GSM already resolved (endpoint='%s' player='%s') — calling TravelToServer directly"),
					*Endpoint, *PlayerName);
				SetLoadingProgressTarget(88.f, FString::Printf(TEXT("Connecting to %s..."), *Endpoint));
				GI->ClearPendingCrossServerTravel();
				if (IsValid(ServerTravelComponent))
				{
					ServerTravelComponent->TravelToServer(Endpoint, PlayerName, RoleId);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[LoadingPC] BeginPlay: ServerTravelComponent is null — cannot complete cross-server travel"));
				}
			}
			else if (GI->HasPendingCrossServerTravel())
			{
				UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: login-flow GSM pending (player='%s') — binding to OnCrossServerTravelReady"),
					*GI->PendingCrossServerPlayerName);
				SetLoadingProgressTarget(30.f, TEXT("Resolving game server..."));
				CrossServerReadyHandle  = GI->OnCrossServerTravelReady.AddUObject(this, &ALoadingPlayerController::OnCrossServerTravelReady);
				CrossServerFailedHandle = GI->OnCrossServerTravelFailed.AddUObject(this, &ALoadingPlayerController::OnCrossServerTravelFailed);
			}
			else
			{
				// Same-server path: LoadingGameMode handles the next OpenLevel call.
				// Portal-GSM path: server will push a ClientTravel when the GSM resolves.
				UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: no Dest and no pending GSM travel — same-server or portal-GSM path"));
			}
		}
		return;
	}

	const FString PlayerName = UGameplayStatics::ParseOption(OptionsString, TEXT("PName"));
	const FName RoleId(*UGameplayStatics::ParseOption(OptionsString, TEXT("Role")));

	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] BeginPlay: cross-server Dest='%s' PName='%s' — calling TravelToServer"),
		*Dest, *PlayerName);
	SetLoadingProgressTarget(88.f, FString::Printf(TEXT("Connecting to %s..."), *Dest));

	if (IsValid(ServerTravelComponent))
	{
		ServerTravelComponent->TravelToServer(Dest, PlayerName, RoleId);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPC] BeginPlay: ServerTravelComponent is null — cannot complete cross-server travel"));
	}
}

void ALoadingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadingProgressTimerHandle);
		if (UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>())
		{
			Bridge->OnConnectionChanged.RemoveDynamic(this, &ALoadingPlayerController::HandleWebUIConnectionChanged);
		}
	}
	DestroyLoadingWidget();

	if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
	{
		UnbindCrossServerDelegates(GI);
	}
	Super::EndPlay(EndPlayReason);
}

void ALoadingPlayerController::EnsureLoadingWidget()
{
	if (LoadingWidget && IsValid(LoadingWidget))
	{
		return;
	}

	LoadingWidget = CreateWidget<UWebUIWidget>(this, UWebUIWidget::StaticClass());
	if (!LoadingWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPC] EnsureLoadingWidget: failed to create AuraWebUI loading widget"));
		return;
	}

	LoadingWidget->HtmlAssetPath = TEXT("WebUI/loading.html");
	if (UWorld* World = GetWorld())
	{
		if (UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>())
		{
			Bridge->OnConnectionChanged.AddDynamic(this, &ALoadingPlayerController::HandleWebUIConnectionChanged);
		}
	}
	LoadingWidget->AddToViewport(0);
	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] EnsureLoadingWidget: created and shown AuraWebUI loading page"));
}

void ALoadingPlayerController::SetLoadingProgressTarget(float InTargetPercent, const FString& InMessage)
{
	LoadingTargetPercent = FMath::Clamp(InTargetPercent, 0.f, 100.f);
	LoadingProgressMessage = InMessage;

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(LoadingProgressTimerHandle))
		{
			World->GetTimerManager().SetTimer(LoadingProgressTimerHandle, this, &ALoadingPlayerController::AdvanceLoadingProgress, 0.05f, true);
		}
	}

	SendLoadingProgress();
}

void ALoadingPlayerController::SendLoadingProgress()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UWebUIBridgeSubsystem* Bridge = World->GetSubsystem<UWebUIBridgeSubsystem>();
	if (!Bridge || !Bridge->IsServerRunning())
	{
		return;
	}

	const int32 DisplayPercent = FMath::Clamp(FMath::RoundToInt(LoadingDisplayedPercent), 0, 100);
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("percent"), DisplayPercent);
	Payload->SetStringField(TEXT("message"), LoadingProgressMessage);

	FString PayloadJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);
	Writer->Close();
	Bridge->SendEvent(TEXT("loading_progress"), PayloadJson);
	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] Web UI progress updated: %s (%d%%)"), *LoadingProgressMessage, DisplayPercent);
}

void ALoadingPlayerController::HandleWebUIConnectionChanged(bool bConnected)
{
	if (bConnected)
	{
		SendLoadingProgress();
	}
}

void ALoadingPlayerController::AdvanceLoadingProgress()
{
	const float Step = 1.5f;
	if (LoadingDisplayedPercent < LoadingTargetPercent)
	{
		LoadingDisplayedPercent = FMath::Min(LoadingDisplayedPercent + Step, LoadingTargetPercent);
		SendLoadingProgress();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadingProgressTimerHandle);
	}
}

void ALoadingPlayerController::DestroyLoadingWidget()
{
	if (LoadingWidget && IsValid(LoadingWidget))
	{
		LoadingWidget->RemoveFromParent();
		LoadingWidget = nullptr;
	}
}

void ALoadingPlayerController::OnCrossServerTravelReady(const FString& Endpoint, const FString& PlayerName)
{
	UE_LOG(LogTemp, Display, TEXT("[LoadingPC] OnCrossServerTravelReady: endpoint='%s' player='%s' — calling TravelToServer"),
		*Endpoint, *PlayerName);

	FName RoleId = NAME_None;
	if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
	{
		RoleId = GI->PendingCrossServerRoleId;
		// Consume the cached result so a stale endpoint cannot be replayed by a
		// later LoadingPC BeginPlay (e.g. on a subsequent reconnect attempt).
		GI->ClearPendingCrossServerTravel();
		UnbindCrossServerDelegates(GI);
	}

	if (IsValid(ServerTravelComponent))
	{
		ServerTravelComponent->TravelToServer(Endpoint, PlayerName, RoleId);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingPC] OnCrossServerTravelReady: ServerTravelComponent is null"));
	}
}

void ALoadingPlayerController::OnCrossServerTravelFailed(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("[LoadingPC] OnCrossServerTravelFailed: '%s' — traveling back to Login level"), *ErrorMessage);

	if (UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>())
	{
		GI->ClearPendingCrossServerTravel();
		UnbindCrossServerDelegates(GI);
	}

	ClientTravel(UServerTravelComponent::LoginLevelPath, TRAVEL_Absolute);
}

void ALoadingPlayerController::UnbindCrossServerDelegates(UAuraGameInstance* GI)
{
	if (CrossServerReadyHandle.IsValid())
	{
		GI->OnCrossServerTravelReady.Remove(CrossServerReadyHandle);
		CrossServerReadyHandle.Reset();
	}
	if (CrossServerFailedHandle.IsValid())
	{
		GI->OnCrossServerTravelFailed.Remove(CrossServerFailedHandle);
		CrossServerFailedHandle.Reset();
	}
}
