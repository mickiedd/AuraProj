// Copyright Druid Mechanics

#include "Actor/LevelJumpPortrail.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Aura/AuraLogChannels.h"
#include "Game/AuraGameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"

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
			UE_LOG(LogAura, Display, TEXT("[JumpPortrail] ClientTravel to destination server: actor=%s playerController=%s destination=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PlayerController),
				*DestinationServer);
			PlayerController->ClientTravel(DestinationServer, TRAVEL_Absolute);
			return;
		}

		UE_LOG(LogAura, Warning, TEXT("[JumpPortrail] DestinationServer is set but no PlayerController resolved from overlap actor=%s"),
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
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationMap);
		return;
	}

	if (!DestinationMapAssetName.IsEmpty())
	{
		UE_LOG(LogAura, Display, TEXT("[JumpPortrail] Traveling via DestinationMapAssetName=%s"), *DestinationMapAssetName);
		UGameplayStatics::OpenLevel(this, FName(DestinationMapAssetName));
		return;
	}

	UE_LOG(LogAura, Error, TEXT("[JumpPortrail] No destination configured on actor=%s"), *GetNameSafe(this));
}
