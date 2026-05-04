#include "SExecFlowGraphNode.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Kismet2/KismetEditorUtilities.h"

// (no file-local helpers needed — route labels moved to output pins)

// -----------------------------------------------------------------------
//  SExecFlowGraphNode
// -----------------------------------------------------------------------

void SExecFlowGraphNode::Construct(const FArguments& InArgs, UExecFlowGraphNode* InNode)
{
	check(InNode);
	GraphNode = InNode;
	UpdateGraphNode();
}

void SExecFlowGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	LeftNodeBox.Reset();
	RightNodeBox.Reset();

	UExecFlowGraphNode* ExecNode = CastChecked<UExecFlowGraphNode>(GraphNode);
	const FExecBPGroup& Group    = ExecNode->GroupData;

	const FLinearColor TitleColor = ExecNode->GetNodeTitleColor();

	// With per-function grouping each group normally has exactly one entry.
	// Build the single function row (or an empty placeholder).
	TSharedPtr<SWidget> FuncContent;
	if (Group.Functions.Num() > 0)
	{
		TSharedPtr<SVerticalBox> FuncList;
		SAssignNew(FuncList, SVerticalBox);
		for (const FExecFuncEntry& Entry : Group.Functions)
		{
			FuncList->AddSlot()
			.AutoHeight()
			.Padding(2.f, 1.f)
			[ BuildFuncRow(Entry) ];
		}
		FuncContent = FuncList;
	}
	else
	{
		FuncContent = SNew(STextBlock)
			.Text(FText::FromString(TEXT("(empty)")))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	}

	// ---- Outer node widget (matches the sketch: BP label on top, function card inside) ----
	GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SHorizontalBox)

		// Left pin rail
		+ SHorizontalBox::Slot()
		.AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)
		[ SAssignNew(LeftNodeBox, SVerticalBox) ]

		// Center body
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Graph.Node.Body"))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.92f))
			.Padding(0.f)
			[
				SNew(SVerticalBox)

				// ---- BP name header (outer container label, like the sketch) ----
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
					.BorderBackgroundColor(TitleColor)
					.Padding(FMargin(12.f, 5.f, 12.f, 5.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Group.BlueprintName))
						.TextStyle(FAppStyle::Get(), "Graph.Node.NodeTitle")
						.ColorAndOpacity(FLinearColor::White)
					]
				]

				// ---- Function card (inner box, like the inner rounded rect in the sketch) ----
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 5.f, 6.f, 7.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
					.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.85f))
					.Padding(FMargin(8.f, 4.f))
					[
						FuncContent.ToSharedRef()
					]
				]
			]
		]

		// Right pin rail
		+ SHorizontalBox::Slot()
		.AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center)
		[ SAssignNew(RightNodeBox, SVerticalBox) ]
	];

	SGraphNode::CreatePinWidgets();
}

TSharedRef<SWidget> SExecFlowGraphNode::BuildFuncRow(const FExecFuncEntry& Entry)
{
	// Kind icon
	FString KindIcon;
	switch (Entry.Kind)
	{
		case EExecNodeKind::Event:           KindIcon = TEXT("[Ev]"); break;
		case EExecNodeKind::CustomEvent:     KindIcon = TEXT("[CE]"); break;
		case EExecNodeKind::EventDispatcher: KindIcon = TEXT("[ED]"); break;
		case EExecNodeKind::Macro:           KindIcon = TEXT("[M]");  break;
		case EExecNodeKind::ExecStep:        KindIcon = TEXT("[S]");  break;
		default:                              KindIcon = TEXT("[F]");  break;
	}

	FString DisplayName = Entry.DisplayName.IsEmpty() ? Entry.FunctionName.ToString() : Entry.DisplayName;
	if (Entry.bIsCycleTruncated) DisplayName = TEXT("(!) ") + DisplayName;

	// Text colour — function name is the main visual focus now
	FSlateColor NameColor = FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f));
	FSlateColor NodeIconColor = FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f));
	if (Entry.Kind == EExecNodeKind::Macro)
	{
		NameColor = FSlateColor(FLinearColor(0.60f, 0.60f, 0.60f));
		NodeIconColor = NameColor;
	}
	if (Entry.Kind == EExecNodeKind::EventDispatcher)
	{
		NameColor = FSlateColor(FLinearColor(0.40f, 0.90f, 1.00f));
		NodeIconColor = NameColor;
	}
	if (Entry.Kind == EExecNodeKind::ExecStep)
	{
		NameColor = FSlateColor(FLinearColor(0.78f, 0.78f, 0.78f));
		NodeIconColor = NameColor;
	}
	if (Entry.bIsRoot)
	{
		NameColor = FSlateColor(FLinearColor(1.00f, 0.95f, 0.50f));
	}
	if (Entry.bIsCycleTruncated)
	{
		NameColor = FSlateColor(FLinearColor(1.00f, 0.45f, 0.20f));
		NodeIconColor = NameColor;
	}

	// Function name is now the primary text — slightly larger
	const FSlateFontInfo NameFont = (Entry.Kind == EExecNodeKind::Macro || Entry.Kind == EExecNodeKind::EventDispatcher)
		? FCoreStyle::GetDefaultFontStyle("Italic",  10)
		: (Entry.Kind == EExecNodeKind::ExecStep
			? FCoreStyle::GetDefaultFontStyle("Regular", 9)
			: FCoreStyle::GetDefaultFontStyle("Bold", 10));

	const FSlateFontInfo IconFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	FText ToolTip = FText::GetEmpty();
	if (Entry.bIsCycleTruncated)
		ToolTip = FText::FromString(TEXT("Cycle detected — traversal stopped here"));
	else if (Entry.bIsRoot)
		ToolTip = FText::FromString(TEXT("This is the node you right-clicked on"));
	if (!Entry.IntraGraphExecPath.IsEmpty())
	{
		const FString BaseTip = ToolTip.IsEmpty() ? FString() : ToolTip.ToString() + TEXT("\n");
		ToolTip = FText::FromString(BaseTip + TEXT("Exec path: ") + Entry.IntraGraphExecPath);
	}
	if (Entry.OutgoingRouteLabels.Num() > 0)
	{
		const FString BaseTip = ToolTip.IsEmpty() ? FString() : ToolTip.ToString() + TEXT("\n");
		ToolTip = FText::FromString(BaseTip + TEXT("Fan-out: ") + FString::Join(Entry.OutgoingRouteLabels, TEXT(", ")));
	}

	TSharedPtr<FExecFuncEntry> EntryPtr = MakeShared<FExecFuncEntry>(Entry);
	UExecFlowGraphNode*        OwnerNode = CastChecked<UExecFlowGraphNode>(GraphNode);

	TSharedRef<SVerticalBox> NameColumn = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayName))
			.Font(NameFont)
			.ColorAndOpacity(NameColor)
		];

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(2.f, 1.f))
		.ToolTipText(ToolTip)
		.Cursor(EMouseCursor::Hand)
		.OnClicked_Lambda([EntryPtr, OwnerNode]() -> FReply
		{
			if (UEdGraphNode* Node = EntryPtr->SourceNode.Get())
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
			else if (UBlueprint* BP = OwnerNode->GroupData.SourceBlueprint.Get())
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(BP);
			return FReply::Handled();
		})
		[
			SNew(SHorizontalBox)

			// Kind badge
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 6.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(KindIcon))
				.Font(IconFont)
				.ColorAndOpacity(NodeIconColor)
			]

			// Function name — primary identity
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				NameColumn
			]
		];
}

// -----------------------------------------------------------------------
//  FExecFlowGraphNodeFactory
// -----------------------------------------------------------------------

TSharedPtr<SGraphNode> FExecFlowGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UExecFlowGraphNode* ExecNode = Cast<UExecFlowGraphNode>(Node))
		return SNew(SExecFlowGraphNode, ExecNode);
	return nullptr;
}

