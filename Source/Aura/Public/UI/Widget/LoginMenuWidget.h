// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "UI/Widget/AuraUserWidget.h"
#include "LoginMenuWidget.generated.h"

class ALoginPlayerController;
class UButton;
class UComboBoxString;

/**
 * Native base class for WBP_LoginMenu.
 * Owns level dropdown and connect button behavior for manual dedicated-server connect flow.
 */
UCLASS()
class AURA_API ULoginMenuWidget : public UAuraUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeDestruct() override;

	bool InitializeForPlayerController(ALoginPlayerController* InOwnerController);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ComboBoxList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConnectBtn;

private:
	struct FLoginMenuServerTarget
	{
		FString DisplayName;
		FString LevelId;
		FString MapPath;
		int32 ServerPort = 0;
		int32 QueryPort = 0;
	};

	UFUNCTION()
	void HandleLevelSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleConnectButtonClicked();

	bool LoadServerTargetsFromLevelConfig();
	const FLoginMenuServerTarget* FindServerTargetByDisplayName(const FString& DisplayName) const;

	TWeakObjectPtr<ALoginPlayerController> OwnerLoginPlayerController;
	TArray<FLoginMenuServerTarget> AvailableServerTargets;
};
