// Copyright CJ 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "ExecFlowGraph.h"

class SGraphEditor;

// Draws cluster background rectangles behind the graph editor,
// aligned to graph-space coordinates using the graph editor's transform.
class SExecFlowClusterOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SExecFlowClusterOverlay)
		: _FlowGraph(nullptr)
	{}
	SLATE_ARGUMENT(UExecFlowGraph*, FlowGraph)
	SLATE_ARGUMENT(TSharedPtr<SGraphEditor>, GraphEditorWidget)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D::ZeroVector; // fills parent via overlay
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
private:
	TObjectPtr<UExecFlowGraph>     FlowGraph;
	TWeakPtr<SGraphEditor>         GraphEditorWidget;
};