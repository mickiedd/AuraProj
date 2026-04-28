// Copyright Druid Mechanics


#include "UI/HUD/AuraHUD.h"

#include "Engine/Canvas.h"
#include "UI/Widget/AuraUserWidget.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"

UOverlayWidgetController* AAuraHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

UAttributeMenuWidgetController* AAuraHUD::GetAttributeMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (AttributeMenuWidgetController == nullptr)
	{
		AttributeMenuWidgetController = NewObject<UAttributeMenuWidgetController>(this, AttributeMenuWidgetControllerClass);
		AttributeMenuWidgetController->SetWidgetControllerParams(WCParams);
		AttributeMenuWidgetController->BindCallbacksToDependencies();
	}
	return AttributeMenuWidgetController;
}

USpellMenuWidgetController* AAuraHUD::GetSpellMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (SpellMenuWidgetController == nullptr)
	{
		SpellMenuWidgetController = NewObject<USpellMenuWidgetController>(this, SpellMenuWidgetControllerClass);
		SpellMenuWidgetController->SetWidgetControllerParams(WCParams);
		SpellMenuWidgetController->BindCallbacksToDependencies();
	}
	return SpellMenuWidgetController;
}

void AAuraHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_AuraHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_AuraHUD"));
	
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UAuraUserWidget>(Widget);
	
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	OverlayWidget->SetWidgetController(WidgetController);
	WidgetController->BroadcastInitialValues();
	Widget->AddToViewport();
}

void AAuraHUD::ToggleLocationDisplay()
{
	bShowLocation = !bShowLocation;
}

void AAuraHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bShowLocation) return;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector Loc = Pawn->GetActorLocation();
	const FString LocText = FString::Printf(TEXT("Location: X=%.1f  Y=%.1f  Z=%.1f"), Loc.X, Loc.Y, Loc.Z);

	const float PosX = Canvas ? Canvas->SizeX * 0.01f : 20.f;
	const float PosY = Canvas ? Canvas->SizeY * 0.5f : 40.f;
	DrawText(LocText, FColor::Yellow, PosX, PosY, GEngine->GetLargeFont(), 1.f);
}

