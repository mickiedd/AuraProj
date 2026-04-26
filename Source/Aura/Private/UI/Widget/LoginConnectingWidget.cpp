// Copyright Druid Mechanics

#include "UI/Widget/LoginConnectingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Animation/WidgetAnimation.h"

void ULoginConnectingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create the main canvas panel if not already created
	UCanvasPanel* RootCanvas = WidgetTree ? WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas")) : nullptr;
	if (RootCanvas)
	{
		WidgetTree->RootWidget = RootCanvas;

		// Create the status text block
		StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
		if (StatusTextBlock)
		{
			StatusTextBlock->SetText(FText::FromString(ConnectingMessage));
			StatusTextBlock->SetFont(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16));
			StatusTextBlock->SetColorAndOpacity(FLinearColor::White);

			// Add to canvas and position in bottom-right
			UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(StatusTextBlock);
			if (CanvasSlot)
			{
				CanvasSlot->SetPosition(FVector2D(-10, -10));
				CanvasSlot->SetSize(FVector2D(300, 50));
				CanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f)); // Bottom-right anchor
				CanvasSlot->SetAutoSize(true);
			}
		}
	}

	// Call blueprint event for custom setup
	OnCreateStatusText();

	// Hide initially
	HideConnecting();
}

void ULoginConnectingWidget::NativeDestruct()
{
	StatusTextBlock = nullptr;
	Super::NativeDestruct();
}

void ULoginConnectingWidget::ShowConnecting(const FString& InMessage)
{
	if (!IsValid(this)) return;

	ConnectingMessage = InMessage;

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(ConnectingMessage));
	}

	SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Display, TEXT("LoginConnectingWidget: Showing - %s"), *ConnectingMessage);
}

void ULoginConnectingWidget::HideConnecting()
{
	if (!IsValid(this)) return;

	SetVisibility(ESlateVisibility::Hidden);

	UE_LOG(LogTemp, Display, TEXT("LoginConnectingWidget: Hidden"));
}

void ULoginConnectingWidget::UpdateMessage(const FString& InMessage)
{
	if (!IsValid(this)) return;

	ConnectingMessage = InMessage;

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(ConnectingMessage));
	}

	UE_LOG(LogTemp, Display, TEXT("LoginConnectingWidget: Message updated - %s"), *ConnectingMessage);
}
