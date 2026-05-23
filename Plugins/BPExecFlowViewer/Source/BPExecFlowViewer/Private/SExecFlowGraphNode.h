// Copyright CJ 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "EdGraphUtilities.h"
#include "ExecFlowGraph.h"

// -----------------------------------------------------------------------
//  Custom SGraphNode widget for UExecFlowGraphNode.
//
//  Layout (left-to-right):
//    [Input pin] | [Title bar + Function rows body] | [Output pin]
//
//  Each function row is a clickable button — single click navigates
//  to the source UEdGraphNode in the Blueprint Editor.
// -----------------------------------------------------------------------
class SExecFlowGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SExecFlowGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UExecFlowGraphNode* InNode);

	// SGraphNode overrides
	virtual void UpdateGraphNode() override;

private:
	/** Build one clickable row for a function / event / macro entry. */
	TSharedRef<SWidget> BuildFuncRow(const FExecFuncEntry& Entry);
};

// -----------------------------------------------------------------------
//  Factory — registered in the module so SGraphEditor uses our widget
//  for every UExecFlowGraphNode it encounters.
// -----------------------------------------------------------------------
class FExecFlowGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override;
};




