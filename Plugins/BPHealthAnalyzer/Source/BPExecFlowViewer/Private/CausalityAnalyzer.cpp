#include "CausalityAnalyzer.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

// -----------------------------------------------------------------------
//  PopulateDataEdges
// -----------------------------------------------------------------------
void FCausalityAnalyzer::PopulateDataEdges(FExecFlowMap& FlowMap)
{
	FlowMap.DataEdges.Reset();

	// Build a reverse lookup: source UEdGraphNode* → (GroupIdx, FuncIdx)
	TMap<UEdGraphNode*, TTuple<int32, int32>> NodeToEntry;
	NodeToEntry.Reserve(FlowMap.Groups.Num() * 2);

	for (int32 GIdx = 0; GIdx < FlowMap.Groups.Num(); ++GIdx)
	{
		const FExecBPGroup& Group = FlowMap.Groups[GIdx];
		for (int32 FIdx = 0; FIdx < Group.Functions.Num(); ++FIdx)
		{
			if (UEdGraphNode* SrcNode = Group.Functions[FIdx].SourceNode.Get())
				NodeToEntry.Add(SrcNode, MakeTuple(GIdx, FIdx));
		}
	}

	// Deduplication key: (srcG, srcF, tgtG, tgtF)
	TSet<TTuple<int32, int32, int32, int32>> Seen;

	for (int32 TargetGIdx = 0; TargetGIdx < FlowMap.Groups.Num(); ++TargetGIdx)
	{
		const FExecBPGroup& Group = FlowMap.Groups[TargetGIdx];
		for (int32 TargetFIdx = 0; TargetFIdx < Group.Functions.Num(); ++TargetFIdx)
		{
			UEdGraphNode* TargetNode = Group.Functions[TargetFIdx].SourceNode.Get();
			if (!TargetNode) continue;

			// For each data input pin, walk backward through pure nodes until
			// we reach a node that is also present in the flow map.
			for (UEdGraphPin* InputPin : TargetNode->Pins)
			{
				if (!InputPin) continue;
				if (InputPin->Direction != EGPD_Input) continue;
				if (InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (InputPin->LinkedTo.Num() == 0) continue;

				TArray<UEdGraphPin*> Frontier;
				TSet<UEdGraphPin*>   VisitedPins;
				Frontier.Add(InputPin);

				while (Frontier.Num() > 0)
				{
					UEdGraphPin* Current = Frontier.Pop(/*bAllowShrinking=*/false);
					if (VisitedPins.Contains(Current)) continue;
					VisitedPins.Add(Current);

					for (UEdGraphPin* Linked : Current->LinkedTo)
					{
						if (!Linked) continue;
						UEdGraphNode* UpstreamNode = Linked->GetOwningNodeUnchecked();
						if (!UpstreamNode) continue;

						const TTuple<int32, int32>* Found = NodeToEntry.Find(UpstreamNode);
						if (Found)
						{
							// Upstream node is in the flow map — record the edge
							const int32 SrcG = Found->Get<0>();
							const int32 SrcF = Found->Get<1>();
							const TTuple<int32,int32,int32,int32> Key =
								MakeTuple(SrcG, SrcF, TargetGIdx, TargetFIdx);
							if (!Seen.Contains(Key))
							{
								Seen.Add(Key);
								FDataFlowEdge Edge;
								Edge.SourceGroupIdx    = SrcG;
								Edge.SourceFuncIdx     = SrcF;
								Edge.TargetGroupIdx    = TargetGIdx;
								Edge.TargetFuncIdx     = TargetFIdx;
								Edge.TargetInputPinName = InputPin->PinName;
								FlowMap.DataEdges.Add(Edge);
							}
						}
						else
						{
							// Pure/intermediate node — traverse its data inputs too
							for (UEdGraphPin* UpPin : UpstreamNode->Pins)
							{
								if (!UpPin) continue;
								if (UpPin->Direction != EGPD_Input) continue;
								if (UpPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
								if (!VisitedPins.Contains(UpPin))
									Frontier.Add(UpPin);
							}
						}
					}
				}
			}
		}
	}
}

// -----------------------------------------------------------------------
//  ComputeChain
// -----------------------------------------------------------------------
FCausalityResult FCausalityAnalyzer::ComputeChain(
	const FExecFlowMap& FlowMap,
	int32 SelectedGroupIdx,
	int32 SelectedFuncIdx)
{
	FCausalityResult Result;

	TArray<TTuple<int32, int32>> Queue;

	auto Enqueue = [&](int32 GIdx, int32 FIdx)
	{
		const TTuple<int32, int32> Key = MakeTuple(GIdx, FIdx);
		if (!Result.CausalNodes.Contains(Key))
		{
			Result.CausalNodes.Add(Key);
			Queue.Add(Key);
		}
	};

	// Seed: the selected entry itself is always causal
	Enqueue(SelectedGroupIdx, SelectedFuncIdx);

	while (Queue.Num() > 0)
	{
		const TTuple<int32, int32> Current = Queue.Pop(/*bAllowShrinking=*/false);
		const int32 CurG = Current.Get<0>();
		const int32 CurF = Current.Get<1>();

		// Walk backward through data edges
		for (int32 DIdx = 0; DIdx < FlowMap.DataEdges.Num(); ++DIdx)
		{
			const FDataFlowEdge& DE = FlowMap.DataEdges[DIdx];
			if (DE.TargetGroupIdx != CurG || DE.TargetFuncIdx != CurF) continue;

			Result.CausalDataEdgeIndices.Add(DIdx);
			Enqueue(DE.SourceGroupIdx, DE.SourceFuncIdx);
		}
	}

	return Result;
}