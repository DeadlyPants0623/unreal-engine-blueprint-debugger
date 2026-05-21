#include "SExecFlowClusterOverlay.h"

#include "GraphEditor.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogExecFlowClusterOverlay, Log, All);

void SExecFlowClusterOverlay::Construct(const FArguments& InArgs)
{
	FlowGraph         = InArgs._FlowGraph;
	GraphEditorWidget = InArgs._GraphEditorWidget;
	SetVisibility(EVisibility::HitTestInvisible);

	UE_LOG(LogExecFlowClusterOverlay, Log, TEXT("Construct: FlowGraph=%s, GraphEditor=%s"),
		FlowGraph ? TEXT("valid") : TEXT("NULL"),
		InArgs._GraphEditorWidget.IsValid() ? TEXT("valid") : TEXT("NULL"));
}

int32 SExecFlowClusterOverlay::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	if (!FlowGraph)
	{
		UE_LOG(LogExecFlowClusterOverlay, Warning, TEXT("OnPaint: FlowGraph is null — skipping"));
		return LayerId;
	}

	if (FlowGraph->ClusterVisuals.Num() == 0)
	{
		UE_LOG(LogExecFlowClusterOverlay, Log, TEXT("OnPaint: ClusterVisuals is empty"));
		return LayerId;
	}

	TSharedPtr<SGraphEditor> GraphEditor = GraphEditorWidget.Pin();
	if (!GraphEditor.IsValid())
	{
		UE_LOG(LogExecFlowClusterOverlay, Warning, TEXT("OnPaint: GraphEditor weak ptr expired"));
		return LayerId;
	}

	const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
	if (!WhiteBrush)
	{
		UE_LOG(LogExecFlowClusterOverlay, Error, TEXT("OnPaint: WhiteBrush not found in AppStyle"));
		return LayerId;
	}

	// Get current pan offset and zoom from the graph editor (UE5.7 FVector2f API)
	FVector2f ViewOffset = FVector2f::ZeroVector;
	float ZoomAmount     = 1.0f;
	GraphEditor->GetViewLocation(ViewOffset, ZoomAmount);

	// Convert a graph-space point to local panel-space pixel position
	auto GraphToLocal = [&](const FVector2D& GraphPos) -> FVector2f
	{
		return (FVector2f(GraphPos) - ViewOffset) * ZoomAmount;
	};

	UE_LOG(LogExecFlowClusterOverlay, Verbose, TEXT("OnPaint: Painting %d clusters — ViewOffset=(%.1f,%.1f) Zoom=%.2f"),
		FlowGraph->ClusterVisuals.Num(), ViewOffset.X, ViewOffset.Y, ZoomAmount);

	for (const FExecFlowClusterVisual& Cluster : FlowGraph->ClusterVisuals)
	{
		const FVector2f LocalMin  = GraphToLocal(Cluster.Min);
		const FVector2f LocalMax  = GraphToLocal(Cluster.Max);
		const FVector2f LocalSize = LocalMax - LocalMin;

		UE_LOG(LogExecFlowClusterOverlay, Verbose,
			TEXT("  Cluster '%s': GraphMin=(%.1f,%.1f) LocalMin=(%.1f,%.1f) LocalSize=(%.1f,%.1f)"),
			*Cluster.Key, Cluster.Min.X, Cluster.Min.Y, LocalMin.X, LocalMin.Y, LocalSize.X, LocalSize.Y);

		if (LocalSize.X <= 1.0f || LocalSize.Y <= 1.0f)
		{
			UE_LOG(LogExecFlowClusterOverlay, Warning,
				TEXT("  Cluster '%s' has degenerate local size (%.1f x %.1f) — skipped"),
				*Cluster.Key, LocalSize.X, LocalSize.Y);
			continue;
		}

		// Fill
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform(LocalMin)),
			WhiteBrush,
			ESlateDrawEffect::None,
			Cluster.FillColor
		);

		// Top border accent
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			TArray<FVector2D>{
				FVector2D(LocalMin),
				FVector2D(LocalMax.X, LocalMin.Y)
			},
			ESlateDrawEffect::None,
			Cluster.BorderColor,
			true,
			2.0f
		);

		// Label
		const FVector2f LabelOffset = LocalMin + FVector2f(8.0f, 4.0f);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform(LabelOffset)),
			FText::FromString(Cluster.Label),
			FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(9.0f * ZoomAmount)),
			ESlateDrawEffect::None,
			Cluster.BorderColor
		);
	}

	return LayerId + 2;
}

FReply SExecFlowClusterOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply SExecFlowClusterOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply SExecFlowClusterOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply SExecFlowClusterOverlay::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}
