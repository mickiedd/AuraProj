// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AuraHUD.generated.h"

class UAttributeMenuWidgetController;
class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UAuraUserWidget;
struct FWidgetControllerParams;
class USpellMenuWidgetController;
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
	void InitializeSkillPanelWebUI(APlayerController* PC, UOverlayWidgetController* WidgetController);
	void SetNativeSkillGlobeVisibility(bool bVisible);
	void SetNativeVitalsAndActionVisibility(bool bVisible);
	bool TriggerNativeOverlayButton(const TArray<FName>& CandidateNames, const TCHAR* CommandName);
	void SendVitalsToWebUI();

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

	FString BuildAbilityIconDataUri(const UTexture2D* Icon);
	bool TryGetWebAbilityInputTag(const FString& InputTagName, FGameplayTag& OutInputTag) const;
	bool bShowLocation = false;


private:

	UPROPERTY()
	TObjectPtr<UAuraUserWidget>  OverlayWidget;	

	UPROPERTY(EditAnywhere)
	TSubclassOf<UAuraUserWidget> OverlayWidgetClass;

	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;

	/** Enable the responsive partial web skill panel alongside the native HUD. */
	UPROPERTY(EditAnywhere, Category = "Web UI|Skill Panel")
	bool bEnableSkillPanelWebUI = true;

	/** Web UI host for the bottom skill panel. Native health/mana remain as fallback. */
	UPROPERTY()
	TObjectPtr<UWebUIWidget> SkillPanelWebUI;

	UPROPERTY()
	TObjectPtr<UWebUIBridgeSubsystem> WebUIBridge;

	/** Cache icon data URLs because ability broadcasts can repeat on replication. */
	TMap<FString, FString> AbilityIconDataUriCache;

	/** Native spell globes are hidden only after the browser confirms the web panel is ready. */
	bool bWebSkillPanelReady = false;

	/** Last replicated vitals, replayed when the Web UI reports that it is ready. */
	float WebHealth = 0.f;
	float WebMaxHealth = 0.f;
	float WebMana = 0.f;
	float WebMaxMana = 0.f;
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
