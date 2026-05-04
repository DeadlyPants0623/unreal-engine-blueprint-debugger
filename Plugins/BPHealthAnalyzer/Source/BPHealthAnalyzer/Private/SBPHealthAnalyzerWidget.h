#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "BPAnalyzerLogic.h"

class SBPHealthAnalyzerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBPHealthAnalyzerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Analysis
	FReply OnAnalyzeClicked();
	FReply OnClearIgnoreListClicked();

	// List row
	TSharedRef<ITableRow> GenerateIssueRow(
		TSharedPtr<FBPIssue> Issue,
		const TSharedRef<STableViewBase>& OwnerTable
	);

	// Navigation — single click only selects, double-click navigates (Fix 2)
	void NavigateToIssue(TSharedPtr<FBPIssue> Issue);
	void OnIssueSelected(TSharedPtr<FBPIssue> Issue, ESelectInfo::Type SelectType);
	void OnIssueDoubleClicked(TSharedPtr<FBPIssue> Issue);

	// Right-click context menu
	TSharedPtr<SWidget> OnContextMenuOpening();
	void OnIgnoreSelectedIssue();
	void OnRemoveFromIgnoreList(); // Fix 2: un-ignore a specific entry
	void OnCopySelectedIssue(); // Feature 1: copy issue details to clipboard

	// Ignore list view toggle (Fix 2)
	FReply OnToggleIgnoredView();
	FText  GetToggleButtonText() const;

	// Ignore list helpers
	FString MakeIssueKey(const FBPIssue& Issue) const;
	void LoadIgnoreList();
	void SaveIgnoreList();
	void SyncDisplayList(); // rebuilds DisplayList from active/ignored based on current toggle

	// Three arrays: active issues, ignored issues, and what the list view actually shows
	TArray<TSharedPtr<FBPIssue>> IssueList;
	TArray<TSharedPtr<FBPIssue>> IgnoredIssueList;
	TArray<TSharedPtr<FBPIssue>> DisplayList; // IssueListView always points here

	TSharedPtr<SListView<TSharedPtr<FBPIssue>>> IssueListView;

	TSet<FString> IgnoreList;
	bool bShowingIgnoredList = false;
};