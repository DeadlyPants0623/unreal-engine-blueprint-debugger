#include "SExecLocalPathWidget.h"
#include "CrossBPExecTracer.h"
#include "ExecFlowGraph.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraphNode.h"

#define LOCTEXT_NAMESPACE "SExecLocalPathWidget"

// -----------------------------------------------------------------------
//  Construct
// -----------------------------------------------------------------------
void SExecLocalPathWidget::Construct(const FArguments& InArgs)
{
	FlowGraph = UExecFlowGraph::Create(GetTransientPackage());

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Control bar ----
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)

			// Backward depth control
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BackwardDepth", "Backward Depth"))
					.Font(FAppStyle::GetFontStyle("NormalText"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SSpinBox<int32>)
					.Value_Lambda([this]() { return BackwardDepth; })
					.MinValue(0)
					.MaxValue(12)
					.MinSliderValue(0)
					.MaxSliderValue(12)
					.MinDesiredWidth(120.f)
					.Delta(1)
					.OnValueChanged_Lambda([this](int32 NewVal) { BackwardDepth = FMath::Clamp(NewVal, 0, 12); })
				]
			]

			// Forward depth control
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 14.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ForwardDepth", "Forward Depth"))
					.Font(FAppStyle::GetFontStyle("NormalText"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0.f, 0.f, 0.f)
				.VAlign(VAlign_Center)
				[
					SNew(SSpinBox<int32>)
					.Value_Lambda([this]() { return ForwardDepth; })
					.MinValue(0)
					.MaxValue(12)
					.MinSliderValue(0)
					.MaxSliderValue(12)
					.MinDesiredWidth(120.f)
					.Delta(1)
					.OnValueChanged_Lambda([this](int32 NewVal) { ForwardDepth = FMath::Clamp(NewVal, 0, 12); })
				]
			]

			// Rebuild button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RebuildButton", "Rebuild"))
				.ToolTipText(LOCTEXT("RebuildTip", "Refresh execution flow graph"))
				.OnClicked(this, &SExecLocalPathWidget::OnRebuildClicked)
			]
		]

		// ---- Graph area ----
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SAssignNew(GraphContainer, SBox)
			[
				CreateGraphEditorWidget()
			]
		]

		// ---- Error message overlay ----
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Visibility(this, &SExecLocalPathWidget::GetErrorVisibility)
			.Padding(4.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return ErrorText; })
				.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.6f))
				.WrapTextAt(500.f)
			]
		]
	];
}

// -----------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------
void SExecLocalPathWidget::SetTargetNode(UEdGraphNode* InNode)
{
	TargetNode = InNode;
	Rebuild();
}

// -----------------------------------------------------------------------
//  Destructor
// -----------------------------------------------------------------------
SExecLocalPathWidget::~SExecLocalPathWidget()
{
	FlowGraph = nullptr;
}

// -----------------------------------------------------------------------
//  Rebuild graph
// -----------------------------------------------------------------------
void SExecLocalPathWidget::Rebuild()
{
	bHasError = false;
	ErrorText = FText::GetEmpty();

	UEdGraphNode* Node = TargetNode.Get();
	if (!Node)
	{
		bHasError = true;
		ErrorText = LOCTEXT("NoNode", "No node selected.");
		if (GraphContainer.IsValid())
			GraphContainer->SetContent(CreateGraphEditorWidget());
		return;
	}

	// Trace local execution flow (same graph only)
	FExecFlowMap FlowMap = FCrossBPExecTracer::TraceFromNode(Node, BackwardDepth, ForwardDepth);
	if (FlowMap.Groups.Num() == 0)
	{
		bHasError = true;
		ErrorText = LOCTEXT("NothingTraced", "Failed to trace execution flow from this node.");
		if (GraphContainer.IsValid())
			GraphContainer->SetContent(CreateGraphEditorWidget());
		return;
	}

	// Populate the flow graph
	if (!UExecFlowGraph::PopulateGraph(FlowGraph, FlowMap))
	{
		bHasError = true;
		ErrorText = LOCTEXT("PopulateFailed", "Failed to populate flow graph visualization.");
		if (GraphContainer.IsValid())
			GraphContainer->SetContent(CreateGraphEditorWidget());
		return;
	}

	// Refresh the SGraphEditor
	if (GraphContainer.IsValid())
	{
		GraphContainer->SetContent(CreateGraphEditorWidget());
		PostProcessWithGraphEditor();
	}
}

TSharedRef<SWidget> SExecLocalPathWidget::CreateGraphEditorWidget()
{
	if (!FlowGraph)
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("NoGraph", "Graph not initialized."))
			.ColorAndOpacity(FLinearColor::Red);
	}

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("CornerLabel", "Execution Flow");

	SAssignNew(GraphEditor, SGraphEditor)
		.AdditionalCommands(nullptr)
		.IsEditable(true)
		.Appearance(Appearance)
		.GraphToEdit(FlowGraph);

	return GraphEditor.ToSharedRef();
}

void SExecLocalPathWidget::PostProcessWithGraphEditor()
{
	if (!GraphEditor.IsValid() || !FlowGraph)
	{
		return;
	}

	// First run Unreal's targeted straighten on each concrete edge.
	for (UEdGraphNode* Node : FlowGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin)
				{
					GraphEditor->StraightenConnections(Pin, LinkedPin);
				}
			}
		}
	}

	// Then run Unreal's global straighten as a cleanup sweep.
	GraphEditor->ClearSelectionSet();
	for (UEdGraphNode* Node : FlowGraph->Nodes)
	{
		if (Node)
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
	GraphEditor->OnStraightenConnections();
	GraphEditor->ClearSelectionSet();

	FlowGraph->NotifyGraphChanged();
}

EVisibility SExecLocalPathWidget::GetErrorVisibility() const
{
	return bHasError ? EVisibility::Visible : EVisibility::Collapsed;
}

// -----------------------------------------------------------------------
//  Controls
// -----------------------------------------------------------------------
FReply SExecLocalPathWidget::OnRebuildClicked()
{
	Rebuild();
	return FReply::Handled();
}


// -----------------------------------------------------------------------
//  FGCObject
// -----------------------------------------------------------------------
void SExecLocalPathWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (FlowGraph)
		Collector.AddReferencedObject(FlowGraph);
}

FString SExecLocalPathWidget::GetReferencerName() const
{
	return TEXT("SExecLocalPathWidget");
}

#undef LOCTEXT_NAMESPACE

