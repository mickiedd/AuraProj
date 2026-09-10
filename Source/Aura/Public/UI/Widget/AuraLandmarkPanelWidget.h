// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/AuraPlayerController.h"
#include "AuraLandmarkPanelWidget.generated.h"

class UBorder;
class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class AAuraPlayerController;
class UAuraLandmarkButton;

/** Native MVP panel; it remains usable before an authored HUD widget is available. */
UCLASS()
class AURA_API UAuraLandmarkPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializePanel(AAuraPlayerController* InController);
	void RefreshCatalog();

	UFUNCTION(BlueprintCallable, Category="Landmark")
	void ShowPanel(bool bShow);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();

	UFUNCTION()
	void HandleLandmarkClicked(FName LandmarkId);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleStopClicked();

	UFUNCTION()
	void HandleGuideStateChanged(FName LandmarkId, EAuraLandmarkGuideState State, float RemainingDistance, const FString& Reason);

	FText StateToText(EAuraLandmarkGuideState State, float RemainingDistance, const FString& Reason) const;

	TWeakObjectPtr<AAuraPlayerController> Controller;
	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY()
	TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY()
	TObjectPtr<UScrollBox> ScrollBox;
	UPROPERTY()
	TObjectPtr<UButton> CloseButton;
	UPROPERTY()
	TObjectPtr<UButton> StopButton;
};
