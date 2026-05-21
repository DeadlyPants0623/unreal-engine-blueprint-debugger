#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/GCObject.h"
#include "ExecFlowGraph.h"
#include "ExecFlowTypes.h"
#include "CausalityAnalyzer.h"

class UEdGraphNode;
class SGraphEditor;
class SBox;

// -----------------------------------------------------------------------
//  SExecLocalPathWidget
//
//  Graph-only execution flow viewer with cross-blueprint support.
//
//  Features:
//    - Detects CallFunction nodes and traces into other blueprints
//    - Shows complete execution flow from any selected node
//    - Forward/backward depth controls for traversal scope
//    - Implements FGCObject to keep UExecFlowGraph alive
//
//  Layout:
//    [Controls: Depth ↕, Rebuild] 
//    [SGraphEditor showing cross-BP flow]
// -----------------------------------------------------------------------
class SExecLocalPathWidget : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SExecLocalPathWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SExecLocalPathWidget() override;

	/** Trace and display execution flow from the selected node. */
	void SetTargetNode(UEdGraphNode* InNode);

	// FGCObject — keeps FlowGraph alive
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	void Rebuild();
	void SetupRerootCallbacks();
	TSharedRef<SWidget> CreateGraphEditorWidget();
	void PostProcessWithGraphEditor();
	EVisibility GetErrorVisibility() const;
	EVisibility GetClearCausalityVisibility() const;

	FReply OnRebuildClicked();
	FReply OnClearCausalityClicked();

	// Called by each node's CausalityCallback. Toggles chain on/off.
	void OnCausalityClicked(int32 OrigGroupIdx, int32 OrigFuncIdx);
	void ClearCausalChain();
	// Applies bIsInCausalChain / bIsDimmedByCausality to every graph node.
	void ApplyCausalHighlighting();

	// ---- Members ----
	TObjectPtr<UExecFlowGraph>   FlowGraph;
	TSharedPtr<SBox>             GraphContainer;
	TSharedPtr<SGraphEditor>     GraphEditor;
	TWeakObjectPtr<UEdGraphNode> TargetNode;

	FText ErrorText;
	bool  bHasError = false;

	int32 ForwardDepth  = 4;
	int32 BackwardDepth = 2;

	TSharedPtr<SOverlay> GraphOverlay;
	TSharedPtr<SWidget>  ClusterOverlay;

	// Last traced flow map — kept alive for causality queries.
	FExecFlowMap CurrentFlowMap;

	// Active causality chain state.
	FCausalityResult ActiveCausalChain;
	bool             bHasActiveCausalChain = false;
	// (OrigGroupIdx, OrigFuncIdx) of the row that triggered the current chain.
	int32 CausalAnchorGroupIdx = INDEX_NONE;
	int32 CausalAnchorFuncIdx  = INDEX_NONE;
};
