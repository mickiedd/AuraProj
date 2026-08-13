// Copyright Druid Mechanics

#include "Actor/LevelJumpPortal.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aura/AuraLogChannels.h"
#include "Game/AuraGameModeBase.h"
#include "Game/ServerTravelComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/AuraPlayerState.h"
#include "Interaction/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"

ALevelJumpPortal::ALevelJumpPortal()
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

void ALevelJumpPortal::BeginPlay()
{
	Super::BeginPlay();

	const float EffectiveMinRadius = FMath::Max(50.f, MinTriggerSphereRadius);
	if (TriggerSphereRadius < EffectiveMinRadius)
	{
		UE_LOG(LogAura, Warning, TEXT("[JumpPortal] TriggerSphereRadius %.1f below min %.1f, clamping."), TriggerSphereRadius, EffectiveMinRadius);
		TriggerSphereRadius = EffectiveMinRadius;
	}
	TriggerSphere->SetSphereRadius(TriggerSphereRadius);

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ALevelJumpPortal::OnTriggerOverlap);
	UE_LOG(LogAura, Display, TEXT("[JumpPortal] BeginPlay actor=%s role=%d remoteRole=%d replicates=%s repMove=%s triggerRadius=%.1f destinationMap=%s destinationMapAssetName=%s startTag=%s"),
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
		UE_LOG(LogAura, Display, TEXT("[JumpPortal] DestinationServer=%s"), *DestinationServer);
	}
	if (!DestinationServerId.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortal] DestinationServerId=%s"), *DestinationServerId);
	}
}

void ALevelJumpPortal::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!HasAuthority() || !IsValid(OtherActor))
	{
		UE_LOG(LogAura, Verbose, TEXT("[JumpPortal] Overlap ignored actor=%s hasAuthority=%s other=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(OtherActor));
		return;
	}

	if (!OtherActor->Implements<UPlayerInterface>())
	{
		UE_LOG(LogAura, Verbose, TEXT("[JumpPortal] Overlap ignored because other actor is not player: %s"), *GetNameSafe(OtherActor));
		return;
	}

	if (bOneShot && bTriggered)
	{
		UE_LOG(LogAura, Log, TEXT("[JumpPortal] One-shot trigger already used. actor=%s"), *GetNameSafe(this));
		return;
	}

	bTriggered = true;
	UE_LOG(LogAura, Display, TEXT("[JumpPortal] Triggered actor=%s by=%s"), *GetNameSafe(this), *GetNameSafe(OtherActor));
	OnJumpPortalTriggered(OtherActor);

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
			UE_LOG(LogAura, Display, TEXT("[JumpPortal] Routing to Loading level for portal GSM query: actor=%s playerController=%s destinationServerId=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PlayerController),
				*DestinationServerId);
			APlayerState* PlayerState = PlayerController->PlayerState.Get();
			const FString SafePlayerName = IsValid(PlayerState) ? PlayerState->GetPlayerName() : FString();
			const FName SafeRoleId = Cast<AAuraPlayerState>(PlayerState) ? CastChecked<AAuraPlayerState>(PlayerState)->GetRole() : NAME_None;
			FString LoadingUrl = FString::Printf(TEXT("%s?PortalServerId=%s"), *UServerTravelComponent::LoadingLevelPath, *DestinationServerId);
			if (!DestinationServer.IsEmpty())
			{
				LoadingUrl += FString::Printf(TEXT("?PortalFallback=%s"), *DestinationServer);
			}
			if (!SafePlayerName.IsEmpty())
			{
				LoadingUrl += FString::Printf(TEXT("?PName=%s"), *SafePlayerName);
			}
			if (!SafeRoleId.IsNone())
			{
				LoadingUrl += FString::Printf(TEXT("?Role=%s"), *SafeRoleId.ToString());
			}
			UE_LOG(LogAura, Display, TEXT("[JumpPortal] ClientTravel to Loading URL=%s"), *LoadingUrl);
			PlayerController->ClientTravel(LoadingUrl, TRAVEL_Absolute);
			return;
		}

		UE_LOG(LogAura, Warning, TEXT("[JumpPortal] DestinationServerId is set but no PlayerController resolved from overlap actor=%s"),
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
			UE_LOG(LogAura, Warning, TEXT("[JumpPortal] Using legacy DestinationServer fallback: %s"), *DestinationServer);
			const AAuraPlayerState* AuraPlayerState = PlayerController->GetPlayerState<AAuraPlayerState>();
			UServerTravelComponent::RouteToServerViaLoadingLevel(PlayerController, DestinationServer,
				IsValid(AuraPlayerState) ? AuraPlayerState->GetPlayerName() : FString(),
				IsValid(AuraPlayerState) ? AuraPlayerState->GetRole() : NAME_None);
			return;
		}

		UE_LOG(LogAura, Warning, TEXT("[JumpPortal] Legacy DestinationServer is set but no PlayerController resolved from overlap actor=%s"),
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
		UE_LOG(LogAura, Warning, TEXT("[JumpPortal] GameMode missing while attempting save/travel. actor=%s"), *GetNameSafe(this));
	}

	IPlayerInterface::Execute_SaveProgress(OtherActor, DestinationPlayerStartTag);

	if (!DestinationMap.IsNull())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortal] Traveling via DestinationMap=%s"), *DestinationMap.ToString());
		UServerTravelComponent::RouteToMapBySoftPtrViaLoadingLevel(this, DestinationMap);
		return;
	}

	if (!DestinationMapAssetName.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortal] Traveling via DestinationMapAssetName=%s"), *DestinationMapAssetName);
		UServerTravelComponent::RouteToMapViaLoadingLevel(this, DestinationMapAssetName);
		return;
	}

	UE_LOG(LogAura, Error, TEXT("[JumpPortal] No destination configured on actor=%s"), *GetNameSafe(this));
}
