// Copyright CJ 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExecFlowTypes.h"

class UEdGraphNode;
class UBlueprint;

/**
 * Synchronous local execution flow tracer.
 *
 * Traces only within the selected node's owning graph and preserves
 * branch/validity exec routes as graph nodes and edges.
 */
class FCrossBPExecTracer
{
public:
	/**
	 * Trace local execution flow from a selected node.
	 *
	 * @param SelectedNode     Node user clicked
	 * @param BackwardDepth    Max upstream hops (0 = no upstream)
	 * @param ForwardDepth     Max downstream hops (0 = no downstream)
	 */
	static FExecFlowMap TraceFromNode(
		UEdGraphNode* SelectedNode,
		int32 BackwardDepth = 1,
		int32 ForwardDepth = 3
	);
};
