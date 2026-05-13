// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LoadingGameMode.generated.h"

/**
 * Game mode for the intermediate Loading level.
 *
 * On BeginPlay it inspects UAuraGameInstance for a pending same-server map
 * travel that was stored by UServerTravelComponent::RouteToMapViaLoadingLevel
 * or RouteToMapBySoftPtrViaLoadingLevel.  After a short delay (so the loading
 * screen can render at least one frame) it calls OpenLevel to the real
 * destination.
 *
 * Cross-server client travel (connecting to a dedicated server) is handled by
 * ALoadingPlayerController – this game mode does nothing for that path.
 *
 * IMPORTANT: This game mode must be set as the GameMode Override in the
 * Loading level's World Settings (Content/Maps/Loading).  It also sets
 * PlayerControllerClass to ALoadingPlayerController in its constructor so
 * no additional editor wiring is needed for the controller.
 */
UCLASS()
class AURA_API ALoadingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALoadingGameMode();

protected:
	virtual void BeginPlay() override;

	/** Seconds to wait before calling OpenLevel on the destination.
	 *  Keeps the loading screen visible for at least one rendered frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Loading", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float TravelDelaySeconds = 0.1f;
};
