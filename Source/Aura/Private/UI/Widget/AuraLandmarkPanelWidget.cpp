// Copyright Druid Mechanics

#include "UI/Widget/AuraLandmarkPanelWidget.h"
#include "UI/Widget/AuraLandmarkButton.h"
#include "World/AuraLandmarkWorldSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/BorderSlot.h"

void UAuraLandmarkPanelWidget::InitializePanel(AAuraPlayerController* InController)
{
	Controller = InController;
	if (IsConstructed())
	{
		RefreshCatalog();
	}
}

void UAuraLandmarkPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	if (AAuraPlayerController* PC = Controller.Get())
	{
		PC->OnLandmarkGuideStateChanged.AddDynamic(this, &UAuraLandmarkPanelWidget::HandleGuideStateChanged);
	}
	RefreshCatalog();
}

void UAuraLandmarkPanelWidget::NativeDestruct()
{
	if (AAuraPlayerController* PC = Controller.Get())
	{
		PC->OnLandmarkGuideStateChanged.RemoveDynamic(this, &UAuraLandmarkPanelWidget::HandleGuideStateChanged);
	}
	Super::NativeDestruct();
}

void UAuraLandmarkPanelWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Canvas;
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	PanelBorder->SetBrushColor(FLinearColor(0.025f, 0.045f, 0.08f, 0.94f));
	PanelBorder->SetPadding(FMargin(18.f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(PanelBorder);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
	PanelSlot->SetOffsets(FMargin(-390.f, 36.f, 360.f, 440.f));

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Layout"));
	PanelBorder->SetContent(Layout);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("LANDMARKS")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.9f, 1.f, 1.f)));
	Title->SetJustification(ETextJustify::Center);
	Layout->AddChildToVerticalBox(Title);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	StatusText->SetText(FText::FromString(TEXT("Choose a destination")));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = Layout->AddChildToVerticalBox(StatusText)) StatusSlot->SetPadding(FMargin(0.f, 8.f));

	ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LandmarkList"));
	ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
	ScrollBox->AddChild(ListBox);
	if (UVerticalBoxSlot* ListSlot = Layout->AddChildToVerticalBox(ScrollBox)) ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Close"));
	CloseButton->OnClicked.AddDynamic(this, &UAuraLandmarkPanelWidget::HandleCloseClicked);
	UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
	CloseText->SetText(FText::FromString(TEXT("Close  [L]")));
	CloseText->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseText);
	StopButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Stop"));
	StopButton->OnClicked.AddDynamic(this, &UAuraLandmarkPanelWidget::HandleStopClicked);
	UTextBlock* StopText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StopText"));
	StopText->SetText(FText::FromString(TEXT("Stop guidance")));
	StopText->SetJustification(ETextJustify::Center);
	StopButton->AddChild(StopText);
	if (UVerticalBoxSlot* StopSlot = Layout->AddChildToVerticalBox(StopButton)) StopSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	if (UVerticalBoxSlot* CloseSlot = Layout->AddChildToVerticalBox(CloseButton)) CloseSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
}

void UAuraLandmarkPanelWidget::RefreshCatalog()
{
	if (!ListBox || !StatusText) return;
	ListBox->ClearChildren();
	AAuraPlayerController* PC = Controller.Get();
	UAuraLandmarkWorldSubsystem* Registry = PC && PC->GetWorld() ? PC->GetWorld()->GetSubsystem<UAuraLandmarkWorldSubsystem>() : nullptr;
	TArray<FAuraLandmarkDescriptor> Catalog;
	if (Registry) Registry->GetLandmarkCatalog(Catalog);

	if (Catalog.Num() == 0)
	{
		StatusText->SetText(FText::FromString(TEXT("No landmarks are available in this map.")));
		return;
	}
	StatusText->SetText(PC && PC->IsLandmarkGuideActive() ? FText::FromString(TEXT("Guide active")) : FText::FromString(TEXT("Choose a destination")));
	for (const FAuraLandmarkDescriptor& Row : Catalog)
	{
		UAuraLandmarkButton* Button = WidgetTree->ConstructWidget<UAuraLandmarkButton>(UAuraLandmarkButton::StaticClass());
		Button->SetLandmarkId(Row.LandmarkId);
		Button->OnLandmarkClicked.AddDynamic(this, &UAuraLandmarkPanelWidget::HandleLandmarkClicked);
		Button->SetIsEnabled(Row.bAvailable);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(Row.bAvailable ? Row.DisplayName : FText::Format(FText::FromString(TEXT("{0}  ({1})")), Row.DisplayName, FText::FromString(Row.UnavailableReason)));
		Label->SetJustification(ETextJustify::Center);
		Button->AddChild(Label);
		if (UVerticalBoxSlot* RowSlot = ListBox->AddChildToVerticalBox(Button)) RowSlot->SetPadding(FMargin(0.f, 3.f));
	}
}

void UAuraLandmarkPanelWidget::ShowPanel(bool bShow)
{
	SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShow) RefreshCatalog();
}

void UAuraLandmarkPanelWidget::HandleLandmarkClicked(FName LandmarkId)
{
	if (AAuraPlayerController* PC = Controller.Get())
	{
		PC->StartLandmarkGuide(LandmarkId);
	}
}

void UAuraLandmarkPanelWidget::HandleCloseClicked()
{
	ShowPanel(false);
}

void UAuraLandmarkPanelWidget::HandleStopClicked()
{
	if (AAuraPlayerController* PC = Controller.Get())
	{
		PC->CancelLandmarkGuide(TEXT("Stopped by player"));
	}
}

void UAuraLandmarkPanelWidget::HandleGuideStateChanged(FName LandmarkId, EAuraLandmarkGuideState State, float RemainingDistance, const FString& Reason)
{
	if (StatusText) StatusText->SetText(StateToText(State, RemainingDistance, Reason));
}

FText UAuraLandmarkPanelWidget::StateToText(EAuraLandmarkGuideState State, float RemainingDistance, const FString& Reason) const
{
	if (!Reason.IsEmpty() && (State == EAuraLandmarkGuideState::Failed || State == EAuraLandmarkGuideState::Cancelled))
	{
		return FText::FromString(Reason);
	}
	const FString Name = Controller.IsValid() ? Controller->GetActiveLandmarkId().ToString() : FString(TEXT("Landmark"));
	switch (State)
	{
	case EAuraLandmarkGuideState::Facing: return FText::Format(FText::FromString(TEXT("Facing {0}...")), FText::FromString(Name));
	case EAuraLandmarkGuideState::Running: return FText::Format(FText::FromString(TEXT("Running to {0}  •  {1} m")), FText::FromString(Name), FText::AsNumber(FMath::RoundToInt(RemainingDistance / 100.f)));
	case EAuraLandmarkGuideState::AwaitingArrival: return FText::FromString(TEXT("Confirming arrival..."));
	case EAuraLandmarkGuideState::Arrived: return FText::Format(FText::FromString(TEXT("Arrived at {0}")), FText::FromString(Name));
	default: return FText::FromString(TEXT("Choose a destination"));
	}
}
