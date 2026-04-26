// Copyright Druid Mechanics

#include "Game/LoginPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Game/AuraGameInstance.h"
#include "Game/LoadScreenSaveGame.h"
#include "Game/LoginGameMode.h"
#include "UI/Widget/LoginConnectingWidget.h"

void ALoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only execute on client (IsLocalPlayerController returns true only for local clients)
	if (IsLocalPlayerController() && bAutoConnectToServer && !bConnectionAttempted)
	{
		bConnectionAttempted = true;

		// Create the connecting widget if class is set
		if (ConnectingWidgetClass)
		{
			ConnectingWidget = CreateWidget<ULoginConnectingWidget>(this, ConnectingWidgetClass);
			if (ConnectingWidget)
			{
				ConnectingWidget->AddToViewport(1);
				ConnectingWidget->ShowConnecting(TEXT("Connecting to server..."));
				UE_LOG(LogTemp, Display, TEXT("LoginPlayerController: Connecting widget created and shown"));
			}
		}

		// Add a small delay to ensure UI and networking are fully initialized
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ConnectionTimerHandle,
				this,
				&ALoginPlayerController::ExecuteClientConnect,
				0.5f,
				false
			);
		}
	}
}

void ALoginPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Attempt connection when possessed (in case BeginPlay didn't trigger)
	if (IsLocalPlayerController() && bAutoConnectToServer && !bConnectionAttempted)
	{
		bConnectionAttempted = true;

		// Create the connecting widget if class is set
		if (ConnectingWidgetClass)
		{
			ConnectingWidget = CreateWidget<ULoginConnectingWidget>(this, ConnectingWidgetClass);
			if (ConnectingWidget)
			{
				ConnectingWidget->AddToViewport(1);
				ConnectingWidget->ShowConnecting(TEXT("Connecting to server..."));
			}
		}

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ConnectionTimerHandle,
				this,
				&ALoginPlayerController::ExecuteClientConnect,
				0.5f,
				false
			);
		}
	}
}

void ALoginPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(ConnectionTimerHandle);
		TimerManager.ClearTimer(HideConnectingWidgetTimerHandle);
	}

	if (ConnectingWidget && IsValid(ConnectingWidget))
	{
		ConnectingWidget->RemoveFromParent();
		ConnectingWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ALoginPlayerController::ExecuteClientConnect()
{
	if (IsLocalPlayerController())
	{
		if (ConnectingWidget)
		{
			ConnectingWidget->UpdateMessage(TEXT("Connecting to server..."));
		}

		// Execute the travel command to connect to the dedicated server
		// Format: open 127.0.0.1?PlayerName=Some_Name
		FString RequestedPlayerName;
		if (UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance()))
		{
			if (!AuraGameInstance->LoadSlotName.IsEmpty() &&
				UGameplayStatics::DoesSaveGameExist(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
			{
				if (USaveGame* SaveObject = UGameplayStatics::LoadGameFromSlot(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
				{
					if (const ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveObject))
					{
						RequestedPlayerName = LoadScreenSaveGame->PlayerName;
					}
				}
			}
		}

		if (RequestedPlayerName.IsEmpty())
		{
			RequestedPlayerName = FPlatformProcess::UserName(false);
		}

		if (RequestedPlayerName.IsEmpty())
		{
			RequestedPlayerName = TEXT("Player");
		}

		RequestedPlayerName.TrimStartAndEndInline();
		RequestedPlayerName.ReplaceInline(TEXT("?"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("&"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("="), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT("#"), TEXT("_"));
		RequestedPlayerName.ReplaceInline(TEXT(" "), TEXT("_"));

		const FString Command = FString::Printf(TEXT("open %s?PlayerName=%s"), *ServerAddress, *RequestedPlayerName);

		UE_LOG(LogTemp, Display, TEXT("LoginPlayerController executing connect command: %s"), *Command);

		ConsoleCommand(*Command);

		// Hide the connecting widget after a delay to show it was successful
		// (or it will be hidden automatically when the level changes)
		TWeakObjectPtr<ALoginPlayerController> WeakThis(this);
		FTimerDelegate HideWidgetDelegate;
		HideWidgetDelegate.BindLambda([WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			ALoginPlayerController* Controller = WeakThis.Get();
			if (Controller->ConnectingWidget && IsValid(Controller->ConnectingWidget))
			{
				Controller->ConnectingWidget->HideConnecting();
			}
		});

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HideConnectingWidgetTimerHandle,
				HideWidgetDelegate,
				3.0f,
				false
			);
		}
	}
}
