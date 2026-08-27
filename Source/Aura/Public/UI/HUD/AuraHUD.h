// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/Anchors.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/AttributeInfo.h"
#include "Combat/AuraTargetingTypes.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "AuraHUD.generated.h"

class UAttributeMenuWidgetController;
class UAttributeSet;
class UAbilitySystemComponent;
struct FWidgetControllerParams;
class USpellMenuWidgetController;
class UTargetInteractionWidgetController;
class UWebUIBridgeSubsystem;
class UWebUIWidget;
class UTexture2D;
/**
 * 
 */
UCLASS()
class AURA_API AAuraHUD : public AHUD
{
	GENERATED_BODY()
public:

	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);
	UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const FWidgetControllerParams& WCParams);
	USpellMenuWidgetController* GetSpellMenuWidgetController(const FWidgetControllerParams& WCParams);

	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	/** Dynamic bridge targets are public so runtime automation can exercise the exact reflected handlers. */
	UFUNCTION()
	void HandleWebUICommand(const FString& Command, const FString& PayloadJson);

	UFUNCTION()
	void HandleWebUIConnectionChanged(bool bConnected);

	/** Runtime diagnostic: number of WebUI menu commands forwarded to native handlers. */
	UFUNCTION(BlueprintPure, Category = "Web UI")
	int32 GetWebHudForwardedActionCount() const { return WebHudForwardedActionCount; }

	void ToggleLocationDisplay();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

private:
	void InitializeWebHUD(APlayerController* PC);
	UWebUIWidget* CreateWebHUDPanel(APlayerController* PC, const FString& HtmlPath, const FAnchors& Anchors, const FMargin& Offsets, const TCHAR* PanelName);
	void SetWebHUDMenuLayout(bool bExpanded);
	void SetWebHUDInteractionLayout(bool bExpanded);
	void SendInitialWebHUDState();
	void SendPlayerProgressToWebUI();
	void SendAttributeCatalogToWebUI();
	void SendSpellCatalogToWebUI();
	void SendInteractionToWebUI(const FAuraTargetDescriptor& Descriptor);
	void SendLocationToWebUI();
	void SendVitalsToWebUI();
	bool ParseWebPayload(const FString& PayloadJson, TSharedPtr<FJsonObject>& OutPayload) const;
	bool IsKnownAbilityTag(const FGameplayTag& AbilityTag) const;
	bool IsKnownAttributeTag(const FGameplayTag& AttributeTag) const;
	bool IsKnownSpellSlot(const FGameplayTag& SlotTag) const;

	UFUNCTION()
	void HandleAbilityInfoForWebUI(const FAuraAbilityInfo& Info);

	UFUNCTION()
	void HandleHealthChangedForWebUI(float NewValue);

	UFUNCTION()
	void HandleMaxHealthChangedForWebUI(float NewValue);

	UFUNCTION()
	void HandleManaChangedForWebUI(float NewValue);

	UFUNCTION()
	void HandleMaxManaChangedForWebUI(float NewValue);

	UFUNCTION()
	void HandleMessageForWebUI(FUIWidgetRow Row);

	UFUNCTION()
	void HandleXPPercentChangedForWebUI(float NewValue);

	UFUNCTION()
	void HandlePlayerLevelChangedForWebUI(int32 NewLevel, bool bLevelUp);

	UFUNCTION()
	void HandleMountedChangedForWebUI(bool bIsMounted);

	UFUNCTION()
	void HandleAttributeInfoForWebUI(const FAuraAttributeInfo& Info);

	UFUNCTION()
	void HandleAttributePointsChangedForWebUI(int32 NewValue);

	UFUNCTION()
	void HandleSpellPointsChangedForWebUI(int32 NewValue);

	UFUNCTION()
	void HandleSpellSelectionForWebUI(bool bSpendPointsButtonEnabled, bool bEquipButtonEnabled, FString DescriptionString, FString NextLevelDescriptionString);

	UFUNCTION()
	void HandleWaitForEquipForWebUI(const FGameplayTag& AbilityType);

	UFUNCTION()
	void HandleStopWaitingForEquipForWebUI(const FGameplayTag& AbilityType);

	UFUNCTION()
	void HandleSpellReassignedForWebUI(const FGameplayTag& AbilityTag);

	UFUNCTION()
	void HandleTargetPreviewForWebUI(const FAuraTargetDescriptor& Descriptor);

	UFUNCTION()
	void HandleTargetPreviewClearedForWebUI();

	FString BuildAbilityIconDataUri(const UTexture2D* Icon);
	FString BuildManualSkillIconDataUri(const FGameplayTag& AbilityTag);
	bool TryGetWebAbilityInputTag(const FString& InputTagName, FGameplayTag& OutInputTag) const;
	bool bShowLocation = false;


private:
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;

	/** Three bounded browser surfaces keep HUD interaction local to its visible panel. */
	UPROPERTY()
	TObjectPtr<UWebUIWidget> WebHUDLeftTop;

	UPROPERTY()
	TObjectPtr<UWebUIWidget> WebHUDRightTop;

	UPROPERTY()
	TObjectPtr<UWebUIWidget> WebHUDBottom;

	UPROPERTY()
	TObjectPtr<UWebUIBridgeSubsystem> WebUIBridge;

	/** Cache icon data URLs because ability broadcasts can repeat on replication. */
	TMap<FString, FString> AbilityIconDataUriCache;
	/** Cache the authored PNG skill icons sent through the WebUI bridge. */
	TMap<FString, FString> ManualSkillIconDataUriCache;

	/** The browser reports ready before cached state is replayed. */
	bool bWebHUDReady = false;
	/** True while WebUI-originated LMB input is held, so browser disconnects can release it safely. */
	bool bWebGameplayLMBDown = false;

	/** Last replicated vitals, replayed when the Web UI reports that it is ready. */
	float WebHealth = 0.f;
	float WebMaxHealth = 0.f;
	float WebMana = 0.f;
	float WebMaxMana = 0.f;
	float WebXPPercent = 0.f;
	int32 WebPlayerLevel = 1;
	int32 WebAttributePoints = 0;
	int32 WebSpellPoints = 0;
	FString LastInteractionPayloadJson;
	FString LastLocationPayloadJson;
	int32 WebHudForwardedActionCount = 0;

	UPROPERTY()
	TObjectPtr<UAttributeMenuWidgetController> AttributeMenuWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UAttributeMenuWidgetController> AttributeMenuWidgetControllerClass;

	UPROPERTY()
	TObjectPtr<USpellMenuWidgetController> SpellMenuWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USpellMenuWidgetController> SpellMenuWidgetControllerClass;

};
