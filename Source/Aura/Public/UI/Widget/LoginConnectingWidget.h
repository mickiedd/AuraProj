// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/AuraUserWidget.h"
#include "LoginConnectingWidget.generated.h"

class UTextBlock;

/**
 * Widget to display server connection status during login.
 * Shown in the bottom-right corner while connecting to the dedicated server.
 */
UCLASS()
class AURA_API ULoginConnectingWidget : public UAuraUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Show the connecting widget with an optional custom message.
	 */
	UFUNCTION(BlueprintCallable, Category = "Login|Connection")
	void ShowConnecting(const FString& InMessage = TEXT("Connecting to server..."));

	/**
	 * Hide the connecting widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "Login|Connection")
	void HideConnecting();

	/**
	 * Update the connection status message.
	 */
	UFUNCTION(BlueprintCallable, Category = "Login|Connection")
	void UpdateMessage(const FString& InMessage);

protected:
	/**
	 * Text widget to display the connection status message.
	 */
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusTextBlock;

	/**
	 * The message displayed during connection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Connection")
	FString ConnectingMessage = TEXT("Connecting to server...");

	/**
	 * Animation or fade-in duration (in seconds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Login|Connection")
	float ShowDuration = 0.3f;

	/**
	 * Create and initialize the status text widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Login|Connection")
	void OnCreateStatusText();
};
