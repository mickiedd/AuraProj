// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "AutoTestResults.h"

class UAutoTestRunnerSubsystem;

/** A single row in the results list. */
struct FAutoTestRow
{
	FString Name;
	FString FilePath;
	FString Description;
	TArray<FString> Tags;
	EAutoTestStatus Status = EAutoTestStatus::NotRun;
	double DurationMs = 0.0;
	TArray<FAutoTestAssertion> Assertions;
	FString Error;

	FString GetStatusText() const;
	FString GetStatusColorHex() const;
	FString GetSummaryLine() const;
};

/**
 * Dockable editor panel: lists discovered tests, run controls, live per-test status,
 * and assertion messages. Polls the active PIE runner each frame for live updates.
 */
class SAutoTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAutoTestPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Find the active PIE runner (for running + live status). */
	UAutoTestRunnerSubsystem* GetPIERunner() const;
	/** Find any runner instance (for discovery display when not in PIE). */
	UAutoTestRunnerSubsystem* GetAnyRunner() const;

	void RefreshRows();
	void RebuildTestList();

	FReply OnRunAllClicked();
	FReply OnRunSelectedClicked();
	FReply OnRefreshClicked();
	FReply OnStopClicked();
	bool IsStopEnabled() const;
	FReply OnRunRowClicked(FString FilePath);

	void OnTestDoubleClicked(TSharedPtr<FAutoTestRow> Row);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FAutoTestRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FText GetSummaryText() const;

	TArray<TSharedPtr<FAutoTestRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FAutoTestRow>>> ListView;
	TSharedPtr<FAutoTestRow> SelectedRow;

	TSharedPtr<class SMultiLineEditableTextBox> DetailBox;
	FString DetailText;

	// Cache of last-seen run identity to avoid rebuilding every frame.
	bool bListDirty = true;
	int32 LastTestCount = -1;
	FString LastReportPath;
};