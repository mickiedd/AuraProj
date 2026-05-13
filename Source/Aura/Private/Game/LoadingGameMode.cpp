// Copyright Druid Mechanics

#include "Game/LoadingGameMode.h"
#include "Game/LoadingPlayerController.h"
#include "Game/AuraGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

ALoadingGameMode::ALoadingGameMode()
{
	// Guarantee that the Loading level always uses ALoadingPlayerController
	// so the ?Dest= URL option is parsed even without editor World Settings wiring.
	PlayerControllerClass = ALoadingPlayerController::StaticClass();

	UE_LOG(LogTemp, Display, TEXT("[LoadingGM] Constructor: PlayerControllerClass set to ALoadingPlayerController"));
}

void ALoadingGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Display, TEXT("[LoadingGM] BeginPlay: world=%s"), *GetNameSafe(GetWorld()));

	UAuraGameInstance* GI = GetGameInstance<UAuraGameInstance>();
	if (!IsValid(GI))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoadingGM] BeginPlay: UAuraGameInstance not found — cannot resolve same-server pending travel"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("[LoadingGM] BeginPlay: HasPendingMapTravel=%d PendingAssetName='%s' PendingSoftMap='%s'"),
		(int32)GI->HasPendingMapTravel(), *GI->PendingMapAssetName, *GI->PendingMapSoftPtr.ToString());

	if (!GI->HasPendingMapTravel())
	{
		// No same-server pending travel.  This is either a cross-server path
		// (handled by ALoadingPlayerController) or a bare visit to this level.
		UE_LOG(LogTemp, Display, TEXT("[LoadingGM] BeginPlay: no pending map travel — cross-server path expected (ALoadingPlayerController will handle Dest option)"));
		return;
	}

	// Capture pending destination and clear it so re-entry is safe.
	const FString MapAssetName   = GI->PendingMapAssetName;
	const TSoftObjectPtr<UWorld> SoftMap = GI->PendingMapSoftPtr;
	GI->ClearPendingMapTravel();

	UE_LOG(LogTemp, Display, TEXT("[LoadingGM] BeginPlay: scheduling same-server travel -> SoftMap='%s'  AssetName='%s'  delay=%.2fs"),
		*SoftMap.ToString(), *MapAssetName, TravelDelaySeconds);

	// Delay so the loading screen widget can render before we transition again.
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(
		TimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, MapAssetName, SoftMap]()
		{
			if (!IsValid(this))
			{
				return;
			}

			if (!SoftMap.IsNull())
			{
				UE_LOG(LogTemp, Display, TEXT("[LoadingGM] Timer fired: OpenLevelBySoftObjectPtr -> %s"), *SoftMap.ToString());
				UGameplayStatics::OpenLevelBySoftObjectPtr(this, SoftMap);
			}
			else if (!MapAssetName.IsEmpty())
			{
				UE_LOG(LogTemp, Display, TEXT("[LoadingGM] Timer fired: OpenLevel -> %s"), *MapAssetName);
				UGameplayStatics::OpenLevel(this, FName(*MapAssetName));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoadingGM] Timer fired: both SoftMap and MapAssetName are empty — no travel performed"));
			}
		}),
		TravelDelaySeconds,
		false
	);
}
