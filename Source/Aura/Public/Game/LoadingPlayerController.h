// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LoadingPlayerController.generated.h"

class UServerTravelComponent;
class UAuraGameInstance;
class UGameServerClient;
class UUserWidget;
class UTextBlock;

/**
 * Player controller used by the intermediate Loading level.
 *
 * On BeginPlay it inspects the world URL for a "Dest" option written by
 * UServerTravelComponent::RouteToServerViaLoadingLevel.  If found it
 * immediately executes the actual cross-server ClientTravel through its
 * ServerTravelComponent so the client transitions to the real destination
 * while the loading screen remains visible until the connection lands.
 *
 * Same-server map travel is handled by ALoadingGameMode; this controller
 * does nothing extra for that path.
 */
UCLASS()
class AURA_API ALoadingPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALoadingPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureLoadingWidget();
	void DestroyLoadingWidget();
	void BindLoadingTipsText();
	void SetLoadingProgressTarget(float InTargetPercent, const FString& InMessage);
	void UpdateLoadingTipsText();
	void AdvanceLoadingProgress();

	/** Called by UAuraGameInstance::OnCrossServerTravelReady when the login-flow GSM query resolves. */
	void OnCrossServerTravelReady(const FString& Endpoint, const FString& PlayerName);

	/** Called by UAuraGameInstance::OnCrossServerTravelFailed; navigates back to the Login level. */
	void OnCrossServerTravelFailed(const FString& ErrorMessage);

	void UnbindCrossServerDelegates(UAuraGameInstance* GI);

	/** Created as a default subobject so cross-server travel through this level
	 *  also benefits from the shared warning/timeout/failure monitoring. */
	UPROPERTY(VisibleAnywhere, Category = "Network")
	TObjectPtr<UServerTravelComponent> ServerTravelComponent;

	/** Keeps the async portal GSM request alive while Loading is displayed. */
	UPROPERTY()
	TObjectPtr<UGameServerClient> GameServerClient;

	/** Widget displayed while the Loading level is active. */
	UPROPERTY(EditDefaultsOnly, Category = "Loading|UI")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	/** Runtime instance of the Loading widget. */
	UPROPERTY()
	TObjectPtr<UUserWidget> LoadingWidget;

	/** Optional text block named TipsText inside WBP_LoadingUI. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TipsTextBlock;

	FTimerHandle LoadingProgressTimerHandle;
	float LoadingDisplayedPercent = 0.f;
	float LoadingTargetPercent = 0.f;
	FString LoadingProgressMessage;

	FDelegateHandle CrossServerReadyHandle;
	FDelegateHandle CrossServerFailedHandle;
};
