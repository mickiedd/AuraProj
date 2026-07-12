// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "SAutoTestPanel.h"
#include "AutoTestRunnerSubsystem.h"
#include "AutoTestLog.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// ===================================================================
// FAutoTestRow helpers
// ===================================================================

FString FAutoTestRow::GetStatusText() const
{
	switch (Status)
	{
	case EAutoTestStatus::NotRun:	return TEXT("Not Run");
	case EAutoTestStatus::Running:	return TEXT("Running");
	case EAutoTestStatus::Pass:		return TEXT("PASS");
	case EAutoTestStatus::Fail:		return TEXT("FAIL");
	case EAutoTestStatus::Timeout:	return TEXT("TIMEOUT");
	case EAutoTestStatus::Error:		return TEXT("ERROR");
	case EAutoTestStatus::Aborted:	return TEXT("ABORTED");
	}
	return TEXT("?");
}

FString FAutoTestRow::GetStatusColorHex() const
{
	switch (Status)
	{
	case EAutoTestStatus::NotRun:	return TEXT("#888888");
	case EAutoTestStatus::Running:	return TEXT("#3399FF");
	case EAutoTestStatus::Pass:		return TEXT("#33CC33");
	case EAutoTestStatus::Fail:		return TEXT("#CC3333");
	case EAutoTestStatus::Timeout:	return TEXT("#CCAA33");
	case EAutoTestStatus::Error:		return TEXT("#CC3333");
	case EAutoTestStatus::Aborted:	return TEXT("#9966CC");
	}
	return TEXT("#888888");
}

FString FAutoTestRow::GetSummaryLine() const
{
	FString Line = FString::Printf(TEXT("[%s] %s  %.1f ms"), *GetStatusText(), *Name, DurationMs);
	if (!Error.IsEmpty())
	{
		Line += TEXT("  — ") + Error;
	}
	return Line;
}

// ===================================================================
// Runner lookup
// ===================================================================

UAutoTestRunnerSubsystem* SAutoTestPanel::GetPIERunner() const
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
		{
			continue;
		}
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UAutoTestRunnerSubsystem* Runner = GI->GetSubsystem<UAutoTestRunnerSubsystem>())
			{
				return Runner;
			}
		}
	}
	return nullptr;
}

UAutoTestRunnerSubsystem* SAutoTestPanel::GetAnyRunner() const
{
	if (UAutoTestRunnerSubsystem* PIE = GetPIERunner())
	{
		return PIE;
	}
	// Fall back to the editor GameInstance's runner (works for discovery without PIE).
	if (GEngine && GEngine->GetWorldContexts().Num() > 0)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					if (UAutoTestRunnerSubsystem* Runner = GI->GetSubsystem<UAutoTestRunnerSubsystem>())
					{
						return Runner;
					}
				}
			}
		}
	}
	return nullptr;
}

// ===================================================================
// Construction
// ===================================================================

void SAutoTestPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Header: run controls + summary.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Run All")))
				.OnClicked(this, &SAutoTestPanel::OnRunAllClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Run Selected")))
				.OnClicked(this, &SAutoTestPanel::OnRunSelectedClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Refresh")))
				.OnClicked(this, &SAutoTestPanel::OnRefreshClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Stop")))
				.ToolTipText(FText::FromString(TEXT("Stop the in-progress suite (current test is marked Aborted)")))
				.IsEnabled(this, &SAutoTestPanel::IsStopEnabled)
				.OnClicked(this, &SAutoTestPanel::OnStopClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(8, 0)
			[
				SNew(STextBlock)
				.Text(this, &SAutoTestPanel::GetSummaryText)
			]
		]

		// Test list.
		+ SVerticalBox::Slot()
		.FillHeight(0.6f)
		.Padding(4)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FAutoTestRow>>)
			.ListItemsSource(&Rows)
			.OnGenerateRow(this, &SAutoTestPanel::OnGenerateRow)
			.OnMouseButtonDoubleClick_Raw(this, &SAutoTestPanel::OnTestDoubleClicked)
			.SelectionMode(ESelectionMode::Single)
		]

		// Detail view.
		+ SVerticalBox::Slot()
		.AutoHeight().Padding(4, 8, 4, 4)
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.4f)
		.Padding(4)
		[
			SAssignNew(DetailBox, SMultiLineEditableTextBox)
			.IsReadOnly(true)
			.Text(FText::GetEmpty())
		]
	];
}

// ===================================================================
// Tick: poll the active runner for live status.
// ===================================================================

void SAutoTestPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UAutoTestRunnerSubsystem* Runner = GetAnyRunner();
	if (!Runner)
	{
		return;
	}

	// Refresh the row set if the discovered count changed or on first frame.
	const int32 TestCount = Runner->GetDiscoveredTests().Num();
	if (TestCount != LastTestCount)
	{
		LastTestCount = TestCount;
		bListDirty = true;
	}

	if (bListDirty)
	{
		RebuildTestList();
		bListDirty = false;
	}

	// Fold last results into rows by name.
	const FAutoTestSuiteResult& Results = Runner->GetLastResults();
	for (const FAutoTestResult& R : Results.Tests)
	{
		for (const TSharedPtr<FAutoTestRow>& Row : Rows)
		{
			if (Row->Name == R.Name || Row->FilePath == R.FilePath)
			{
				Row->Status = R.Status;
				Row->DurationMs = R.DurationMs;
				Row->Assertions = R.Assertions;
				Row->Error = R.Error;
				break;
			}
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}

	// Update the detail box for the selected row.
	if (SelectedRow.IsValid())
	{
		FString NewDetail;
		NewDetail += FString::Printf(TEXT("%s\n%s\n\n"), *SelectedRow->GetSummaryLine(), *SelectedRow->FilePath);
		if (!SelectedRow->Description.IsEmpty())
		{
			NewDetail += SelectedRow->Description + TEXT("\n\n");
		}
		if (SelectedRow->Assertions.Num() == 0)
		{
			NewDetail += TEXT("(no assertions recorded)\n");
		}
		else
		{
			for (const FAutoTestAssertion& A : SelectedRow->Assertions)
			{
				NewDetail += FString::Printf(TEXT("  [%s] %s\n"),
					A.bPassed ? TEXT("OK") : TEXT("X"),
					*A.Message);
			}
		}
		if (!SelectedRow->Error.IsEmpty())
		{
			NewDetail += TEXT("\nError: ") + SelectedRow->Error + TEXT("\n");
		}
		if (NewDetail != DetailText)
		{
			DetailText = NewDetail;
			if (DetailBox.IsValid())
			{
				DetailBox->SetText(FText::FromString(DetailText));
			}
		}
	}
}

// ===================================================================
// Row management
// ===================================================================

void SAutoTestPanel::RebuildTestList()
{
	UAutoTestRunnerSubsystem* Runner = GetAnyRunner();
	if (!Runner)
	{
		Rows.Reset();
		return;
	}

	Rows.Reset();
	for (const FAutoTestInfo& Info : Runner->GetDiscoveredTests())
	{
		TSharedPtr<FAutoTestRow> Row = MakeShared<FAutoTestRow>();
		Row->Name = Info.Name;
		Row->FilePath = Info.FilePath;
		Row->Description = Info.Description;
		Row->Tags = Info.Tags;
		Rows.Add(Row);
	}

	// Fold any existing results.
	const FAutoTestSuiteResult& Results = Runner->GetLastResults();
	for (const FAutoTestResult& R : Results.Tests)
	{
		for (const TSharedPtr<FAutoTestRow>& Row : Rows)
		{
			if (Row->Name == R.Name || Row->FilePath == R.FilePath)
			{
				Row->Status = R.Status;
				Row->DurationMs = R.DurationMs;
				Row->Assertions = R.Assertions;
				Row->Error = R.Error;
				break;
			}
		}
	}
}

void SAutoTestPanel::RefreshRows()
{
	bListDirty = true;
}

TSharedRef<ITableRow> SAutoTestPanel::OnGenerateRow(TSharedPtr<FAutoTestRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor StatusColor(FColor::FromHex(Item->GetStatusColorHex()));
	const FString FilePath = Item->FilePath;

	return SNew(STableRow<TSharedPtr<FAutoTestRow>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 2)
			[
				SNew(SBorder)
				.BorderBackgroundColor(StatusColor)
				.Padding(0)
				[
					SNew(SBox).WidthOverride(12).HeightOverride(12)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4, 2)
			[
				SNew(STextBlock).Text(FText::FromString(Item->GetSummaryLine()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 2)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Run")))
				.OnClicked(FOnClicked::CreateLambda([this, FilePath]()
				{
					return OnRunRowClicked(FilePath);
				}))
			]
		];
}

FText SAutoTestPanel::GetSummaryText() const
{
	if (UAutoTestRunnerSubsystem* Runner = GetAnyRunner())
	{
		if (Runner->IsRunning())
		{
			return FText::FromString(TEXT("Running..."));
		}
		const FAutoTestSuiteResult& R = Runner->GetLastResults();
		if (R.Total > 0)
		{
			return FText::FromString(FString::Printf(
				TEXT("Total %d | Pass %d | Fail %d | Timeout %d | Error %d | Aborted %d"),
				R.Total, R.Passed, R.Failed, R.TimedOut, R.Errored, R.Aborted));
		}
	}
	return FText::FromString(TEXT("Idle"));
}

// ===================================================================
// Button handlers
// ===================================================================

FReply SAutoTestPanel::OnRunAllClicked()
{
	if (UAutoTestRunnerSubsystem* Runner = GetPIERunner())
	{
		Runner->RunAll();
	}
	else
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] No PIE session — start PIE to run tests."));
	}
	return FReply::Handled();
}

FReply SAutoTestPanel::OnRunSelectedClicked()
{
	if (!SelectedRow.IsValid())
	{
		return FReply::Handled();
	}
	if (UAutoTestRunnerSubsystem* Runner = GetPIERunner())
	{
		Runner->RunTest(SelectedRow->FilePath);
	}
	else
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] No PIE session — start PIE to run tests."));
	}
	return FReply::Handled();
}

FReply SAutoTestPanel::OnRefreshClicked()
{
	if (UAutoTestRunnerSubsystem* Runner = GetAnyRunner())
	{
		Runner->Refresh();
		bListDirty = true;
	}
	return FReply::Handled();
}

FReply SAutoTestPanel::OnStopClicked()
{
	if (UAutoTestRunnerSubsystem* Runner = GetPIERunner())
	{
		Runner->StopRun();
	}
	else
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] Stop: no PIE session."));
	}
	return FReply::Handled();
}

bool SAutoTestPanel::IsStopEnabled() const
{
	if (UAutoTestRunnerSubsystem* Runner = GetPIERunner())
	{
		return Runner->IsRunning();
	}
	return false;
}

FReply SAutoTestPanel::OnRunRowClicked(FString FilePath)
{
	if (UAutoTestRunnerSubsystem* Runner = GetPIERunner())
	{
		Runner->RunTest(FilePath);
	}
	else
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] No PIE session — start PIE to run tests."));
	}
	return FReply::Handled();
}

void SAutoTestPanel::OnTestDoubleClicked(TSharedPtr<FAutoTestRow> Row)
{
	SelectedRow = Row;
}