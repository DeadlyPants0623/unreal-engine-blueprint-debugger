#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"

// -----------------------------------------------------------------------
//  Kind of entry inside a Blueprint group node
// -----------------------------------------------------------------------
enum class EExecNodeKind : uint8
{
	Function,
	Event,
	CustomEvent,
	EventDispatcher,
	Macro,
	ExecStep,
};

// -----------------------------------------------------------------------
//  One row inside a Blueprint group node.
// -----------------------------------------------------------------------
struct FExecFuncEntry
{
	FName FunctionName;
	FString DisplayName;
	EExecNodeKind Kind = EExecNodeKind::Function;
	bool bIsRoot = false;
	bool bIsCycleTruncated = false;
	TWeakObjectPtr<UEdGraphNode> SourceNode;
	FString IntraGraphExecPath;
	/** Routes this node fans OUT to (e.g. "Branch: True", "IsValid: Valid"). Owned by this node, not inherited. */
	TArray<FString> OutgoingRouteLabels;
};

// -----------------------------------------------------------------------
//  One Blueprint container node in the flow graph.
// -----------------------------------------------------------------------
struct FExecBPGroup
{
	FString BlueprintName;
	int32 DepthColumn = 0;
	TWeakObjectPtr<UBlueprint> SourceBlueprint;
	TArray<FExecFuncEntry> Functions;
	bool bIsSynthetic = false;
};

// -----------------------------------------------------------------------
//  One directed edge in the flow map, carrying its exec route label.
// -----------------------------------------------------------------------
struct FExecFlowEdge
{
	int32   FromIdx    = INDEX_NONE;
	int32   ToIdx      = INDEX_NONE;
	FString RouteLabel;           // normalised (e.g. "IsValid: Valid", "Branch: True")

	bool operator==(const FExecFlowEdge& O) const
	{
		return FromIdx == O.FromIdx && ToIdx == O.ToIdx && RouteLabel == O.RouteLabel;
	}
};

// -----------------------------------------------------------------------
//  Complete graph result consumed by UExecFlowGraph.
// -----------------------------------------------------------------------
struct FExecFlowMap
{
	TArray<FExecBPGroup>  Groups;
	TArray<FExecFlowEdge> Edges;
	int32 RootGroupIndex = INDEX_NONE;
};
