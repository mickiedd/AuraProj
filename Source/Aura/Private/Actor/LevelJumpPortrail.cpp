// Copyright Druid Mechanics

#include "Actor/LevelJumpPortrail.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aura/AuraLogChannels.h"
#include "Game/GameServerClient.h"
#include "Game/AuraGameModeBase.h"
#include "Game/ServerTravelComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

ALevelJumpPortrail::ALevelJumpPortrail()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(Root);
	PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(Root);
	TriggerSphere->InitSphereRadius(TriggerSphereRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
}

void ALevelJumpPortrail::BeginPlay()
{
	Super::BeginPlay();

	const float EffectiveMinRadius = FMath::Max(50.f, MinTriggerSphereRadius);
	if (TriggerSphereRadius < EffectiveMinRadius)
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] TriggerSphereRadius %.1f below min %.1f, clamping."), TriggerSphereRadius, EffectiveMinRadius);
		TriggerSphereRadius = EffectiveMinRadius;
	}
	TriggerSphere->SetSphereRadius(TriggerSphereRadius);

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ALevelJumpPortrail::OnTriggerOverlap);
	UE_LOG(LogAura, Display, TEXT("[JumpPortrail] BeginPlay actor=%s role=%d remoteRole=%d replicates=%s repMove=%s triggerRadius=%.1f destinationMap=%s destinationMapAssetName=%s startTag=%s"),
		*GetNameSafe(this),
		(int32)GetLocalRole(),
		(int32)GetRemoteRole(),
		GetIsReplicated() ? TEXT("true") : TEXT("false"),
		IsReplicatingMovement() ? TEXT("true") : TEXT("false"),
		TriggerSphere->GetScaledSphereRadius(),
		*DestinationMap.ToString(),
		*DestinationMapAssetName,
		*DestinationPlayerStartTag.ToString());
	if (!DestinationServer.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortrail] DestinationServer=%s"), *DestinationServer);
	}
	if (!DestinationServerId.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortrail] DestinationServerId=%s"), *DestinationServerId);
	}
}

void ALevelJumpPortrail::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!HasAuthority() || !IsValid(OtherActor))
	{
		UE_LOG(LogAura, Verbose, TEXT("[JumpPortrail] Overlap ignored actor=%s hasAuthority=%s other=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(OtherActor));
		return;
	}

	if (!OtherActor->Implements<UPlayerInterface>())
	{
		UE_LOG(LogAura, Verbose, TEXT("[JumpPortrail] Overlap ignored because other actor is not player: %s"), *GetNameSafe(OtherActor));
		return;
	}

	if (bOneShot && bTriggered)
	{
		UE_LOG(LogAura, Log, TEXT("[JumpPortrail] One-shot trigger already used. actor=%s"), *GetNameSafe(this));
		return;
	}

	bTriggered = true;
	UE_LOG(LogAura, Display, TEXT("[JumpPortrail] Triggered actor=%s by=%s"), *GetNameSafe(this), *GetNameSafe(OtherActor));
	OnJumpPortrailTriggered(OtherActor);

	if (!DestinationServerId.IsEmpty())
	{
		APlayerController* PlayerController = nullptr;
		if (const APawn* Pawn = Cast<APawn>(OtherActor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
		}
		if (PlayerController == nullptr)
		{
			PlayerController = Cast<APlayerController>(OtherActor);
		}

		if (IsValid(PlayerController))
		{
			UE_LOG(LogAura, Display, TEXT("[JumpPortrail] Routing to Loading level for portal GSM query: actor=%s playerController=%s destinationServerId=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PlayerController),
				*DestinationServerId);
			APlayerState* PlayerState = PlayerController->PlayerState.Get();
			const FString SafePlayerName = IsValid(PlayerState) ? PlayerState->GetPlayerName() : FString();
			FString LoadingUrl = FString::Printf(TEXT("%s?PortalServerId=%s"), *UServerTravelComponent::LoadingLevelPath, *DestinationServerId);
			if (!DestinationServer.IsEmpty())
			{
				LoadingUrl += FString::Printf(TEXT("?PortalFallback=%s"), *DestinationServer);
			}
			if (!SafePlayerName.IsEmpty())
			{
				LoadingUrl += FString::Printf(TEXT("?PName=%s"), *SafePlayerName);
			}
			UE_LOG(LogAura, Display, TEXT("[JumpPortrail] ClientTravel to Loading URL=%s"), *LoadingUrl);
			PlayerController->ClientTravel(LoadingUrl, TRAVEL_Absolute);
			return;
		}

		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] DestinationServerId is set but no PlayerController resolved from overlap actor=%s"),
			*GetNameSafe(OtherActor));
		return;
	}

	if (!DestinationServer.IsEmpty())
	{
		APlayerController* PlayerController = nullptr;
		if (const APawn* Pawn = Cast<APawn>(OtherActor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
		}
		if (PlayerController == nullptr)
		{
			PlayerController = Cast<APlayerController>(OtherActor);
		}

		if (IsValid(PlayerController))
		{
			UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] Using legacy DestinationServer fallback: %s"), *DestinationServer);
			UServerTravelComponent::RouteToServerViaLoadingLevel(PlayerController, DestinationServer);
			return;
		}

		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] Legacy DestinationServer is set but no PlayerController resolved from overlap actor=%s"),
			*GetNameSafe(OtherActor));
		return;
	}

	const FString DestinationAssetName = !DestinationMap.IsNull()
		? DestinationMap.ToSoftObjectPath().GetAssetName()
		: DestinationMapAssetName;

	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		AuraGameMode->SaveWorldState(GetWorld(), DestinationAssetName);
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] GameMode missing while attempting save/travel. actor=%s"), *GetNameSafe(this));
	}

	IPlayerInterface::Execute_SaveProgress(OtherActor, DestinationPlayerStartTag);

	if (!DestinationMap.IsNull())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortrail] Traveling via DestinationMap=%s"), *DestinationMap.ToString());
		UServerTravelComponent::RouteToMapBySoftPtrViaLoadingLevel(this, DestinationMap);
		return;
	}

	if (!DestinationMapAssetName.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortrail] Traveling via DestinationMapAssetName=%s"), *DestinationMapAssetName);
		UServerTravelComponent::RouteToMapViaLoadingLevel(this, DestinationMapAssetName);
		return;
	}

	UE_LOG(LogAura, Error, TEXT("[JumpPortrail] No destination configured on actor=%s"), *GetNameSafe(this));
}

bool ALevelJumpPortrail::LoadGameServerManagerConfig(FString& OutAddress, int32& OutPort) const
{
	OutAddress = TEXT("127.0.0.1");
	OutPort = 9000;

	TArray<FString> CandidatePaths;
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("ServerConnection.json")));

	FString JsonContent;
	for (const FString& Path : CandidatePaths)
	{
		if (!FPaths::FileExists(Path))
		{
			continue;
		}

		if (FFileHelper::LoadFileToString(JsonContent, *Path))
		{
			TSharedPtr<FJsonObject> RootObject;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonContent), RootObject) && RootObject.IsValid())
			{
				FString ParsedAddress;
				if (RootObject->TryGetStringField(TEXT("gameServerAddress"), ParsedAddress) || RootObject->TryGetStringField(TEXT("serverAddress"), ParsedAddress))
				{
					ParsedAddress.TrimStartAndEndInline();
					if (!ParsedAddress.IsEmpty())
					{
						OutAddress = ParsedAddress;
					}
				}

				double ParsedPort = 0.0;
				if (RootObject->TryGetNumberField(TEXT("gameServerPort"), ParsedPort))
				{
					const int32 PortInt = static_cast<int32>(ParsedPort);
					if (PortInt >= 1 && PortInt <= 65535)
					{
						OutPort = PortInt;
					}
				}
			}

			return true;
		}
	}

	return false;
}

void ALevelJumpPortrail::QueryDestinationServerViaGSM(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] QueryDestinationServerViaGSM skipped due to invalid PlayerController."));
		return;
	}

	if (DestinationServerId.IsEmpty())
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] QueryDestinationServerViaGSM skipped because DestinationServerId is empty."));
		return;
	}

	if (bDestinationServerQueryInFlight)
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] GSM query already in flight for actor=%s"), *GetNameSafe(this));
		return;
	}

	FString GameServerAddress;
	int32 GameServerPort = 0;
	LoadGameServerManagerConfig(GameServerAddress, GameServerPort);

	DestinationGameServerClient = NewObject<UGameServerClient>(this);
	if (!IsValid(DestinationGameServerClient))
	{
		UE_LOG(LogAura, Error, TEXT("[JumpPortrail] Failed to allocate UGameServerClient for GSM query."));
		return;
	}

	bDestinationServerQueryInFlight = true;
	TWeakObjectPtr<ALevelJumpPortrail> WeakThis(this);
	TWeakObjectPtr<APlayerController> WeakPlayerController(PlayerController);

	// Show the Loading level immediately so the player is not stuck in the game
	// level for the duration of the GSM query (~10–30 s).
	// The server-side PlayerController remains connected while the client is on
	// the Loading level, so we can push the final ClientTravel from the callback.
	UE_LOG(LogAura, Display, TEXT("[JumpPortrail] GSM query started — pushing client to Loading level: PC=%s"), *GetNameSafe(PlayerController));
	PlayerController->ClientTravel(UServerTravelComponent::LoadingLevelPath, TRAVEL_Absolute);

	DestinationGameServerClient->RequestServer(
		GameServerAddress,
		GameServerPort,
		DestinationServerId,
		DestinationServerQueryTimeoutSeconds,
		FOnGameServerResponse::CreateLambda([WeakThis, WeakPlayerController](const FGameServerResponse& Response)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			ALevelJumpPortrail* Self = WeakThis.Get();
			Self->bDestinationServerQueryInFlight = false;
			Self->DestinationGameServerClient = nullptr;

			if (!WeakPlayerController.IsValid())
			{
				UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] GSM response arrived but PlayerController is invalid."));
				return;
			}

			APlayerController* TargetPC = WeakPlayerController.Get();
			if (Response.bSuccess)
			{
				const FString DestinationEndpoint = FString::Printf(TEXT("%s:%d"), *Response.Host, Response.Port);
				UE_LOG(LogAura, Display, TEXT("[JumpPortrail] GSM resolved server id '%s' -> %s. Pushing ClientTravel to client on Loading level."),
					*Self->DestinationServerId,
					*DestinationEndpoint);
				// Client is already on the Loading level; server PC is still connected.
				// Call TravelToServer directly (no second trip through Loading).
				APlayerState* PlayerState = IsValid(TargetPC) ? TargetPC->PlayerState.Get() : nullptr;
				const FString PlayerName = IsValid(PlayerState)
					? PlayerState->GetPlayerName() : FString();
				if (UServerTravelComponent* TravelComp = UServerTravelComponent::GetOrCreateFor(TargetPC))
				{
					TravelComp->TravelToServer(DestinationEndpoint, PlayerName);
				}
				return;
			}

			UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] GSM query failed for server id '%s': %s"),
				*Self->DestinationServerId,
				*Response.ErrorMessage);

			if (!Self->DestinationServer.IsEmpty())
			{
				UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] Falling back to legacy DestinationServer=%s"), *Self->DestinationServer);
				APlayerState* PlayerState = IsValid(TargetPC) ? TargetPC->PlayerState.Get() : nullptr;
				const FString PlayerName = IsValid(PlayerState)
					? PlayerState->GetPlayerName() : FString();
				if (UServerTravelComponent* TravelComp = UServerTravelComponent::GetOrCreateFor(TargetPC))
				{
					TravelComp->TravelToServer(Self->DestinationServer, PlayerName);
				}
			}
		}));
}
