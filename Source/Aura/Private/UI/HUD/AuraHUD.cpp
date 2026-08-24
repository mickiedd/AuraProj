// Copyright Druid Mechanics


#include "UI/HUD/AuraHUD.h"

#include "Engine/Canvas.h"
#include "UI/Widget/AuraUserWidget.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "Player/AuraPlayerController.h"

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

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	if (bShowLocation)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			const FVector Loc = Pawn->GetActorLocation();
			const FString LocText = FString::Printf(TEXT("Location: X=%.1f  Y=%.1f  Z=%.1f"), Loc.X, Loc.Y, Loc.Z);
			const float PosX = Canvas ? Canvas->SizeX * 0.01f : 20.f;
			const float PosY = Canvas ? Canvas->SizeY * 0.5f : 40.f;
			DrawText(LocText, FColor::Yellow, PosX, PosY, GEngine->GetLargeFont(), 1.f);
		}
	}

	// The interaction preview is intentionally native so it remains usable even
	// when a project-specific WBP has no bindings yet. The controller still
	// broadcasts the same descriptor for Blueprint UI consumers.
	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(PC))
	{
		const FAuraTargetDescriptor& Descriptor = AuraPC->GetFocusedTargetDescriptor();
		if (Descriptor.IsValid() && Canvas)
		{
			FString Prompt = Descriptor.DisplayName.ToString();
			for (int32 Index = 0; Index < Descriptor.InteractionOptions.Num(); ++Index)
			{
				const FAuraInteractionOption& Option = Descriptor.InteractionOptions[Index];
				Prompt += FString::Printf(TEXT("\n%d. %s%s"), Index + 1, *Option.DisplayText.ToString(), Option.bEnabled ? TEXT("") : TEXT(" (unavailable)"));
			}
			DrawText(Prompt, FColor::White, Canvas->SizeX * 0.70f, Canvas->SizeY * 0.72f, GEngine->GetLargeFont(), 0.8f);
		}
	}
}
