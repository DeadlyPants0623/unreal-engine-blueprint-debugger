#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "ExecFlowTypes.h"

#include "ExecFlowGraph.generated.h"

struct FExecFlowClusterVisual
{
	FString Key;
	FString Label;
	FLinearColor FillColor;
	FLinearColor BorderColor;
	FVector2D Min;
	FVector2D Max;
};

// -----------------------------------------------------------------------
//  Schema — minimal read-only schema so SGraphEditor is satisfied.
// -----------------------------------------------------------------------
UCLASS()
class UExecFlowGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	/** All pins render as white regardless of category. */
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;

	/** Read-only — never allow user-created connections. */
	virtual const FPinConnectionResponse CanCreateConnection(
		const UEdGraphPin* A, const UEdGraphPin* B) const override;

	/** No default value editing needed. */
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override { return true; }

	/** Read-only — ignore break requests. */
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override
	{
	}

	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override
	{
	}
};

// -----------------------------------------------------------------------
//  Node — one node per FExecBPGroup (one Blueprint container per column).
// -----------------------------------------------------------------------
UCLASS()
class UExecFlowGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	/** The Blueprint group this node visualises. Set before AllocateDefaultPins. */
	FExecBPGroup GroupData;

	/** One input exec pin (left edge) and one output exec pin (right edge). */
	virtual void AllocateDefaultPins() override;

	/** Returns the Blueprint name as the node title. */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** Lists all functions/events in the tooltip. */
	virtual FText GetTooltipText() const override;

	/**
	 * Title bar colour:
	 *   Callers  (depth < 0) → blue
	 *   Root     (depth == 0) → gold
	 *   Callees  (depth > 0) → green
	 */
	virtual FLinearColor GetNodeTitleColor() const override;

	virtual bool ShowPaletteIconOnNode() const override { return false; }

	/** Convenience accessors used during edge wiring. */
	UEdGraphPin* GetInputPin() const;
	/** Returns first output pin (fallback). */
	UEdGraphPin* GetOutputPin() const;
	/** Returns the output pin whose name matches the given compact route (e.g. "V", "T").
	 *  Falls back to GetOutputPin() if no exact match is found. */
	UEdGraphPin* GetOutputPinForRoute(const FString& CompactRoute) const;
};

// -----------------------------------------------------------------------
//  Graph — container for all UExecFlowGraphNodes.
// -----------------------------------------------------------------------
UCLASS()
class UExecFlowGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	/** Create a schema-wired graph owned by Outer. */
	static UExecFlowGraph* Create(UObject* Outer);

	/**
	 * Clear the graph and repopulate it from FlowMap.
	 *
	 * Layout:
	 *   X = DepthColumn * 400  (callers left, root centre, callees right)
	 *   Y = lane-based alignment shared across columns for straighter paths,
	 *       with lane choice driven by parent continuity.
	 */
	static bool PopulateGraph(UExecFlowGraph* Graph, const FExecFlowMap& FlowMap);

	TArray<FExecFlowClusterVisual> ClusterVisuals;
};
