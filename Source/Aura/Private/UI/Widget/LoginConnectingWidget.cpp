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
	UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] NativeConstruct: Widget=%s Root=%s BoundStatusText=%s"),
		*GetNameSafe(this),
		WidgetTree ? *GetNameSafe(WidgetTree->RootWidget) : TEXT("null"),
		*GetNameSafe(StatusTextBlock));

	// If a Blueprint did not bind a text block, create a runtime fallback.
	if (!StatusTextBlock && WidgetTree)
	{
		if (!WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		}

		UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (RootCanvas)
		{
			StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
			if (StatusTextBlock)
			{
				UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(StatusTextBlock);
				if (CanvasSlot)
				{
					CanvasSlot->SetAnchors(FAnchors(1.0f, 1.0f));
					CanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f));
					CanvasSlot->SetPosition(FVector2D(-24.0f, -24.0f));
					CanvasSlot->SetAutoSize(true);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[LoginConnWidget] Failed to construct fallback StatusTextBlock"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoginConnWidget] Root widget is not a CanvasPanel, cannot create fallback status text. Root=%s"), *GetNameSafe(WidgetTree->RootWidget));
		}
	}

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(ConnectingMessage));
		StatusTextBlock->SetFont(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16));
		StatusTextBlock->SetColorAndOpacity(FLinearColor::White);
		UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] Status text initialized with message: %s"), *ConnectingMessage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConnWidget] No StatusTextBlock available after construct"));
	}

	// Call blueprint event for custom setup
	OnCreateStatusText();

	// Hide initially
	HideConnecting();
}

void ULoginConnectingWidget::NativeDestruct()
{
	UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] NativeDestruct: Widget=%s"), *GetNameSafe(this));
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
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConnWidget] ShowConnecting: StatusTextBlock is null, message cannot render in text widget"));
	}

	// HitTestInvisible: the status text renders, but the widget (a full-viewport
	// CanvasPanel root) does NOT intercept mouse hits. Otherwise, when this is shown
	// over the login menu (e.g. the mid-game server-lost message on return to Login),
	// it blocks every click on the dropdown / connect button behind it. This widget
	// is a pure status display with no interactive elements.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] Showing: %s"), *ConnectingMessage);
}

void ULoginConnectingWidget::HideConnecting()
{
	if (!IsValid(this)) return;

	SetVisibility(ESlateVisibility::Hidden);

	UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] Hidden"));
}

void ULoginConnectingWidget::UpdateMessage(const FString& InMessage)
{
	if (!IsValid(this)) return;

	ConnectingMessage = InMessage;

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(ConnectingMessage));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginConnWidget] UpdateMessage: StatusTextBlock is null, message cannot render in text widget"));
	}

	UE_LOG(LogTemp, Display, TEXT("[LoginConnWidget] Message updated: %s"), *ConnectingMessage);
}
