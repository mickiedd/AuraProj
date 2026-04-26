// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Game/AuraGameModeBase.h"
#include "LoginGameMode.generated.h"

class ALoginPlayerController;

/**
 * Game mode for the Login map.
 * Automatically connects clients to the dedicated server.
 */
UCLASS()
class AURA_API ALoginGameMode : public AAuraGameModeBase
{
	GENERATED_BODY()

public:
	ALoginGameMode();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
