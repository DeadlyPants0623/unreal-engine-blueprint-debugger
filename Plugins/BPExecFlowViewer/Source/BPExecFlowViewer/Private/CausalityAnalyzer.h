#pragma once

#include "CoreMinimal.h"
#include "ExecFlowTypes.h"

// -----------------------------------------------------------------------
//  Result of a causality query: which (OrigGroupIdx, OrigFuncIdx) pairs
//  in the flow map are data-ancestors of the selected entry, plus which
//  data edges connect them.
// -----------------------------------------------------------------------
struct FCausalityResult
{
	// (OrigGroupIdx, OrigFuncIdx) pairs — includes the selected node itself.
	TSet<TTuple<int32, int32>> CausalNodes;
	// Indices into FExecFlowMap::DataEdges that lie on the causal path.
	TSet<int32> CausalDataEdgeIndices;

	bool IsEmpty() const { return CausalNodes.IsEmpty(); }
};

// -----------------------------------------------------------------------
//  FCausalityAnalyzer
//
//  Two-phase helper:
//    1. PopulateDataEdges  — call once after TraceFromNode to record
//       which exec-flow entries provide data inputs to which others.
//    2. ComputeChain       — call on row click; BFS backward through
//       DataEdges to find all ancestors of the selected entry.
// -----------------------------------------------------------------------
class FCausalityAnalyzer
{
public:
	// Walk every SourceNode's input data pins backward, traversing through
	// pure (non-exec) nodes, and record any connection whose source is
	// also present in the flow map.  Resets FlowMap.DataEdges first.
	static void PopulateDataEdges(FExecFlowMap& FlowMap);

	// BFS backward through DataEdges from (SelectedGroupIdx, SelectedFuncIdx).
	// The selected node is always included in the result.
	static FCausalityResult ComputeChain(
		const FExecFlowMap& FlowMap,
		int32 SelectedGroupIdx,
		int32 SelectedFuncIdx);
};