// Copyright Druid Mechanics

#include "Game/LoginGameMode.h"
#include "Game/LoginPlayerController.h"

ALoginGameMode::ALoginGameMode()
{
	bEnableAuthorityWorldPersistence = false;
	// Set the player controller class for this game mode
	PlayerControllerClass = ALoginPlayerController::StaticClass();
}

void ALoginGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogTemp, Display, TEXT("LoginGameMode initialized for map: %s"), *MapName);
}
