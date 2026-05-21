#include "SExecFlowGraphNode.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Kismet2/KismetEditorUtilities.h"

// -----------------------------------------------------------------------
//  SExecFlowGraphNode
// -----------------------------------------------------------------------

void SExecFlowGraphNode::Construct(const FArguments& InArgs, UExecFlowGraphNode* InNode)
{
	if (!ensureMsgf(InNode, TEXT("SExecFlowGraphNode: null graph node")))
	{
		return;
	}
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

	// Causality color attributes — re-evaluated every paint pass.
	TWeakObjectPtr<UExecFlowGraphNode> WeakNode(ExecNode);

	auto BodyBgColor = TAttribute<FSlateColor>::CreateLambda(
		[WeakNode]() -> FSlateColor
		{
			if (const UExecFlowGraphNode* N = WeakNode.Get())
				if (N->bIsDimmedByCausality)
					return FLinearColor(0.05f, 0.05f, 0.05f, 0.18f);
			return FLinearColor(0.08f, 0.08f, 0.08f, 0.92f);
		});

	auto TitleBgColor = TAttribute<FSlateColor>::CreateLambda(
		[WeakNode, TitleColor]() -> FSlateColor
		{
			if (const UExecFlowGraphNode* N = WeakNode.Get())
			{
				if (N->bIsDimmedByCausality)
					return FLinearColor(TitleColor.R * 0.2f, TitleColor.G * 0.2f, TitleColor.B * 0.2f, 0.25f);
				if (N->bIsInCausalChain)
					return FLinearColor(0.80f, 0.62f, 0.04f, 1.0f);
			}
			return TitleColor;
		});

	auto TitleTextColor = TAttribute<FSlateColor>::CreateLambda(
		[WeakNode]() -> FSlateColor
		{
			if (const UExecFlowGraphNode* N = WeakNode.Get())
				if (N->bIsDimmedByCausality)
					return FLinearColor(0.5f, 0.5f, 0.5f, 0.35f);
			return FLinearColor::White;
		});

	auto FuncCardBgColor = TAttribute<FSlateColor>::CreateLambda(
		[WeakNode]() -> FSlateColor
		{
			if (const UExecFlowGraphNode* N = WeakNode.Get())
				if (N->bIsDimmedByCausality)
					return FLinearColor(0.02f, 0.02f, 0.02f, 0.15f);
			return FLinearColor(0.04f, 0.04f, 0.04f, 0.85f);
		});

	// Build function rows (with FuncIdx for causality button wiring)
	TSharedPtr<SWidget> FuncContent;
	if (Group.Functions.Num() > 0)
	{
		TSharedPtr<SVerticalBox> FuncList;
		SAssignNew(FuncList, SVerticalBox);
		for (int32 FuncIdx = 0; FuncIdx < Group.Functions.Num(); ++FuncIdx)
		{
			FuncList->AddSlot()
			.AutoHeight()
			.Padding(2.f, 1.f)
			[ BuildFuncRow(Group.Functions[FuncIdx], FuncIdx) ];
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

	// ---- Outer node widget ----
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
			.BorderBackgroundColor(BodyBgColor)
			.Padding(0.f)
			[
				SNew(SVerticalBox)

				// BP name header
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
					.BorderBackgroundColor(TitleBgColor)
					.Padding(FMargin(12.f, 5.f, 12.f, 5.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Group.BlueprintName))
						.TextStyle(FAppStyle::Get(), "Graph.Node.NodeTitle")
						.ColorAndOpacity(TitleTextColor)
					]
				]

				// Function card
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 5.f, 6.f, 7.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
					.BorderBackgroundColor(FuncCardBgColor)
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

TSharedRef<SWidget> SExecFlowGraphNode::BuildFuncRow(const FExecFuncEntry& Entry, int32 FuncIdx)
{
	// ---- Static data ----
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

	FLinearColor NameColor(0.95f, 0.95f, 0.95f);
	if (Entry.Kind == EExecNodeKind::Macro)           NameColor = FLinearColor(0.60f, 0.60f, 0.60f);
	if (Entry.Kind == EExecNodeKind::EventDispatcher) NameColor = FLinearColor(0.40f, 0.90f, 1.00f);
	if (Entry.Kind == EExecNodeKind::ExecStep)        NameColor = FLinearColor(0.78f, 0.78f, 0.78f);
	if (Entry.bIsRoot)                                NameColor = FLinearColor(1.00f, 0.95f, 0.50f);
	if (Entry.bIsCycleTruncated)                      NameColor = FLinearColor(1.00f, 0.45f, 0.20f);

	const FSlateFontInfo NameFont = (Entry.Kind == EExecNodeKind::Macro || Entry.Kind == EExecNodeKind::EventDispatcher)
		? FCoreStyle::GetDefaultFontStyle("Italic",  10)
		: (Entry.Kind == EExecNodeKind::ExecStep
			? FCoreStyle::GetDefaultFontStyle("Regular", 9)
			: FCoreStyle::GetDefaultFontStyle("Bold", 10));
	const FSlateFontInfo IconFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	FString Tooltip;
	if (Entry.bIsCycleTruncated)    Tooltip = TEXT("Cycle detected — traversal stopped here");
	else if (Entry.bIsRoot)         Tooltip = TEXT("This is the node you right-clicked on");
	if (!Entry.IntraGraphExecPath.IsEmpty())
	{
		if (!Tooltip.IsEmpty()) Tooltip += TEXT("\n");
		Tooltip += TEXT("Exec path: ") + Entry.IntraGraphExecPath;
	}
	if (Entry.OutgoingRouteLabels.Num() > 0)
	{
		if (!Tooltip.IsEmpty()) Tooltip += TEXT("\n");
		Tooltip += TEXT("Fan-out: ") + FString::Join(Entry.OutgoingRouteLabels, TEXT(", "));
	}

	// ---- Captures for lambdas ----
	const TSharedPtr<FExecFuncEntry> EntryPtr = MakeShared<FExecFuncEntry>(Entry);
	UExecFlowGraphNode* OwnerNode = CastChecked<UExecFlowGraphNode>(GraphNode);
	TWeakObjectPtr<UExecFlowGraphNode> WeakNode(OwnerNode);

	// Dim text when node is causally irrelevant
	auto RowNameColor = TAttribute<FSlateColor>::CreateLambda(
		[WeakNode, NameColor]() -> FSlateColor
		{
			if (const UExecFlowGraphNode* N = WeakNode.Get())
				if (N->bIsDimmedByCausality)
					return FLinearColor(NameColor.R * 0.35f, NameColor.G * 0.35f, NameColor.B * 0.35f, 0.35f);
			return NameColor;
		});

	static const FLinearColor CardBorder(0.04f, 0.04f, 0.04f, 0.85f);

	TSharedRef<SWidget> NameColumn = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.Node.Body"))
		.BorderBackgroundColor(CardBorder)
		.Padding(FMargin(3.f, 1.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayName))
			.Font(NameFont)
			.ColorAndOpacity(RowNameColor)
		];

	// Navigate button — left-click jumps to the source node in the Blueprint Editor
	TSharedRef<SWidget> NavigateButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(2.f, 1.f))
		.ToolTipText(FText::FromString(Tooltip))
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

			// Kind icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(KindIcon))
				.Font(IconFont)
				.ColorAndOpacity(RowNameColor)
			]

			// Function name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				NameColumn
			]
		];

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			NavigateButton
		];

	// Causality button — highlights data ancestors of this row in the flow graph
	if (OwnerNode->CausalityCallback && EntryPtr->SourceNode.IsValid())
	{
		const int32 CapturedFuncIdx = FuncIdx;

		auto CausalBtnColor = TAttribute<FSlateColor>::CreateLambda(
			[WeakNode]() -> FSlateColor
			{
				if (const UExecFlowGraphNode* N = WeakNode.Get())
					if (N->bIsInCausalChain)
						return FLinearColor(1.0f, 0.72f, 0.08f); // amber when active
				return FLinearColor(0.38f, 0.38f, 0.38f); // dim when idle
			});

		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(3.f, 1.f))
			.ToolTipText(FText::FromString(TEXT("Show data ancestors (causality chain)")))
			.Cursor(EMouseCursor::Hand)
			.OnClicked_Lambda([WeakNode, CapturedFuncIdx]() -> FReply
			{
				if (UExecFlowGraphNode* N = WeakNode.Get())
					if (N->CausalityCallback)
						N->CausalityCallback(N->OrigGroupIdx, N->OrigFuncIdx);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("◈"))) // ◈
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CausalBtnColor)
			]
		];
	}

	// Re-root button — re-traces the graph with this entry as the new root
	if (OwnerNode->RerootCallback && EntryPtr->SourceNode.IsValid())
	{
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(3.f, 1.f))
			.ToolTipText(FText::FromString(TEXT("Trace from here")))
			.Cursor(EMouseCursor::Hand)
			.OnClicked_Lambda([EntryPtr, OwnerNode]() -> FReply
			{
				if (OwnerNode->RerootCallback)
					if (UEdGraphNode* Node = EntryPtr->SourceNode.Get())
						OwnerNode->RerootCallback(Node);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("→"))) // →
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.80f, 1.00f)))
			]
		];
	}

	return Row;
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
