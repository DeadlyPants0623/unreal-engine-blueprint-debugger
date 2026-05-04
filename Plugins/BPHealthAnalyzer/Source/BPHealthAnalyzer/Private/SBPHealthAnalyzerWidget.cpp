#include "SBPHealthAnalyzerWidget.h"
#include "BPAnalyzerLogic.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformApplicationMisc.h"

// Config constants
static const TCHAR* BPAnalyzerConfigSection = TEXT("BPHealthAnalyzer");
static const TCHAR* BPAnalyzerConfigKey     = TEXT("IgnoredIssues");

// ------------------------------------------------------------------ Construct

void SBPHealthAnalyzerWidget::Construct(const FArguments& InArgs)
{
    LoadIgnoreList();

    ChildSlot
    [
        SNew(SVerticalBox)

        // Top bar: Analyze + Clear Ignore List + Toggle Ignored View buttons
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.f, 0.f, 8.f, 0.f)
            [
                SNew(SButton)
                .Text(FText::FromString("Analyze All Blueprints"))
                .OnClicked(this, &SBPHealthAnalyzerWidget::OnAnalyzeClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.f, 0.f, 8.f, 0.f)
            [
                SNew(SButton)
                .Text(FText::FromString("Clear Ignore List"))
                .ToolTipText(FText::FromString("Remove all ignored issues and re-run analysis"))
                .OnClicked(this, &SBPHealthAnalyzerWidget::OnClearIgnoreListClicked)
            ]
            // Fix 2: toggle between active issues and ignored issues
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(this, &SBPHealthAnalyzerWidget::GetToggleButtonText)
                .ToolTipText(FText::FromString("Switch between active issues and ignored issues"))
                .OnClicked(this, &SBPHealthAnalyzerWidget::OnToggleIgnoredView)
            ]
        ]

        // Results list — always bound to DisplayList
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        .Padding(8.f)
        [
            SAssignNew(IssueListView, SListView<TSharedPtr<FBPIssue>>)
            .ListItemsSource(&DisplayList)
            .OnGenerateRow(this, &SBPHealthAnalyzerWidget::GenerateIssueRow)
            .OnSelectionChanged(this, &SBPHealthAnalyzerWidget::OnIssueSelected)
            .OnMouseButtonDoubleClick(this, &SBPHealthAnalyzerWidget::OnIssueDoubleClicked)
            .OnContextMenuOpening(this, &SBPHealthAnalyzerWidget::OnContextMenuOpening)
            .HeaderRow
            (
                SNew(SHeaderRow)
                + SHeaderRow::Column("Blueprint").DefaultLabel(FText::FromString("Blueprint")).FillWidth(0.2f)
                + SHeaderRow::Column("Graph").DefaultLabel(FText::FromString("Graph")).FillWidth(0.15f)
                + SHeaderRow::Column("Node").DefaultLabel(FText::FromString("Node")).FillWidth(0.2f)
                + SHeaderRow::Column("Issue").DefaultLabel(FText::FromString("Issue")).FillWidth(0.45f)
            )
        ]
    ];
}

// ------------------------------------------------------------------ Analyze

FReply SBPHealthAnalyzerWidget::OnAnalyzeClicked()
{
    TArray<FBPIssue> RawIssues = FBPAnalyzerLogic::AnalyzeAllBlueprints();

    // Build set of every valid key that currently exists in the project
    TSet<FString> ValidKeys;
    for (const FBPIssue& Issue : RawIssues)
    {
        ValidKeys.Add(MakeIssueKey(Issue));
    }

    // Prune stale ignore entries (nodes that no longer exist)
    int32 CountBefore = IgnoreList.Num();
    for (auto It = IgnoreList.CreateIterator(); It; ++It)
    {
        if (!ValidKeys.Contains(*It)) It.RemoveCurrent();
    }
    if (IgnoreList.Num() != CountBefore) SaveIgnoreList();

    // Split raw results into active and ignored
    IssueList.Empty();
    IgnoredIssueList.Empty();
    for (FBPIssue& Issue : RawIssues)
    {
        if (IgnoreList.Contains(MakeIssueKey(Issue)))
            IgnoredIssueList.Add(MakeShared<FBPIssue>(Issue));
        else
            IssueList.Add(MakeShared<FBPIssue>(Issue));
    }

    SyncDisplayList();
    return FReply::Handled();
}

FReply SBPHealthAnalyzerWidget::OnClearIgnoreListClicked()
{
    IgnoreList.Empty();
    SaveIgnoreList();
    bShowingIgnoredList = false; // return to active view after clearing
    return OnAnalyzeClicked();
}

// ------------------------------------------------------------------ Row generation

TSharedRef<ITableRow> SBPHealthAnalyzerWidget::GenerateIssueRow(
    TSharedPtr<FBPIssue> Issue,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    // Ignored items are displayed grayed out; active items use severity colour
    FSlateColor RowColor = bShowingIgnoredList
        ? FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f))
        : (Issue->Severity == 1
            ? FSlateColor(FLinearColor(1.f, 0.3f, 0.3f))
            : FSlateColor(FLinearColor(1.f, 0.85f, 0.2f)));

    return SNew(STableRow<TSharedPtr<FBPIssue>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.2f)
        [ SNew(STextBlock).Text(FText::FromString(Issue->BlueprintName)).ColorAndOpacity(RowColor) ]
        + SHorizontalBox::Slot().FillWidth(0.15f)
        [ SNew(STextBlock).Text(FText::FromString(Issue->GraphName)).ColorAndOpacity(RowColor) ]
        + SHorizontalBox::Slot().FillWidth(0.2f)
        [ SNew(STextBlock).Text(FText::FromString(Issue->NodeName)).ColorAndOpacity(RowColor) ]
        + SHorizontalBox::Slot().FillWidth(0.45f)
        [ SNew(STextBlock).Text(FText::FromString(Issue->IssueDescription)).ColorAndOpacity(RowColor) ]
    ];
}

// ------------------------------------------------------------------ Navigation

void SBPHealthAnalyzerWidget::NavigateToIssue(TSharedPtr<FBPIssue> Issue)
{
    if (!Issue.IsValid()) return;

    if (UEdGraphNode* Node = Issue->SourceNode.Get())
    {
        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
        return;
    }

    if (UBlueprint* BP = Issue->SourceBlueprint.Get())
    {
        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(BP);
    }
}

void SBPHealthAnalyzerWidget::OnIssueSelected(TSharedPtr<FBPIssue> Issue, ESelectInfo::Type SelectType)
{
    // Single click only selects the row — use double-click to navigate to the node
}

void SBPHealthAnalyzerWidget::OnIssueDoubleClicked(TSharedPtr<FBPIssue> Issue)
{
    NavigateToIssue(Issue);
}

// ------------------------------------------------------------------ Context menu

TSharedPtr<SWidget> SBPHealthAnalyzerWidget::OnContextMenuOpening()
{
    TArray<TSharedPtr<FBPIssue>> Selected = IssueListView->GetSelectedItems();
    if (Selected.Num() == 0) return SNullWidget::NullWidget;

    FMenuBuilder MenuBuilder(true, nullptr);

    if (bShowingIgnoredList)
    {
        // Fix 2: when browsing ignored items, allow removing individual entries
        MenuBuilder.AddMenuEntry(
            FText::FromString("Remove from Ignore List"),
            FText::FromString("Restore this issue to the active list"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &SBPHealthAnalyzerWidget::OnRemoveFromIgnoreList))
        );
    }
    else
    {
        MenuBuilder.AddMenuEntry(
            FText::FromString("Ignore this issue"),
            FText::FromString("Hide this specific issue from future analysis runs (scoped to this Blueprint + node)"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &SBPHealthAnalyzerWidget::OnIgnoreSelectedIssue))
        );
        MenuBuilder.AddMenuEntry(
            FText::FromString("Copy issue details"),
            FText::FromString("Copy a summary of this issue to the clipboard"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &SBPHealthAnalyzerWidget::OnCopySelectedIssue))
        );
    }

    return MenuBuilder.MakeWidget();
}

void SBPHealthAnalyzerWidget::OnIgnoreSelectedIssue()
{
    TArray<TSharedPtr<FBPIssue>> Selected = IssueListView->GetSelectedItems();
    if (Selected.Num() == 0) return;

    for (const TSharedPtr<FBPIssue>& Issue : Selected)
    {
        IgnoreList.Add(MakeIssueKey(*Issue));
        IssueList.Remove(Issue);
        IgnoredIssueList.Add(Issue);
    }

    SaveIgnoreList();
    SyncDisplayList();
}

void SBPHealthAnalyzerWidget::OnRemoveFromIgnoreList()
{
    TArray<TSharedPtr<FBPIssue>> Selected = IssueListView->GetSelectedItems();
    if (Selected.Num() == 0) return;

    for (const TSharedPtr<FBPIssue>& Issue : Selected)
    {
        IgnoreList.Remove(MakeIssueKey(*Issue));
        IgnoredIssueList.Remove(Issue);
        IssueList.Add(Issue); // restore to active list
    }

    SaveIgnoreList();
    SyncDisplayList();
}

void SBPHealthAnalyzerWidget::OnCopySelectedIssue()
{
    TArray<TSharedPtr<FBPIssue>> Selected = IssueListView->GetSelectedItems();
    if (Selected.Num() == 0) return;

    FString CopyText;
    for (const TSharedPtr<FBPIssue>& Issue : Selected)
    {
        CopyText += FString::Printf(
            TEXT("[%s] Blueprint: %s | Graph: %s | Node: %s | %s\n"),
            Issue->Severity == 1 ? TEXT("ERROR") : TEXT("WARNING"),
            *Issue->BlueprintName,
            *Issue->GraphName,
            *Issue->NodeName,
            *Issue->IssueDescription
        );
    }
    FPlatformApplicationMisc::ClipboardCopy(*CopyText);
}

// ------------------------------------------------------------------ Ignore list toggle (Fix 2)

FReply SBPHealthAnalyzerWidget::OnToggleIgnoredView()
{
    bShowingIgnoredList = !bShowingIgnoredList;
    SyncDisplayList();
    return FReply::Handled();
}

FText SBPHealthAnalyzerWidget::GetToggleButtonText() const
{
    if (bShowingIgnoredList)
        return FText::FromString(FString::Printf(TEXT("← Active Issues (%d)"), IssueList.Num()));
    return FText::FromString(FString::Printf(TEXT("Ignored Issues (%d) →"), IgnoredIssueList.Num()));
}

void SBPHealthAnalyzerWidget::SyncDisplayList()
{
    DisplayList = bShowingIgnoredList ? IgnoredIssueList : IssueList;
    if (IssueListView.IsValid()) IssueListView->RequestListRefresh();
}

// ------------------------------------------------------------------ Ignore list persistence

FString SBPHealthAnalyzerWidget::MakeIssueKey(const FBPIssue& Issue) const
{
    return FString::Printf(TEXT("%s|%s|%s|%s"),
        *Issue.BlueprintName, *Issue.GraphName,
        *Issue.NodeName, *Issue.NodeGuid.ToString()
    );
}

void SBPHealthAnalyzerWidget::LoadIgnoreList()
{
    IgnoreList.Empty();
    TArray<FString> SavedKeys;
    GConfig->GetArray(BPAnalyzerConfigSection, BPAnalyzerConfigKey, SavedKeys, GEditorPerProjectIni);
    for (const FString& Key : SavedKeys) IgnoreList.Add(Key);
}

void SBPHealthAnalyzerWidget::SaveIgnoreList()
{
    TArray<FString> KeyArray = IgnoreList.Array();
    GConfig->SetArray(BPAnalyzerConfigSection, BPAnalyzerConfigKey, KeyArray, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}
