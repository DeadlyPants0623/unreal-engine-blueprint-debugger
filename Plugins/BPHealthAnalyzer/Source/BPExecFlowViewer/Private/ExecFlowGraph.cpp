#include "ExecFlowGraph.h"

#include "K2Node_FunctionEntry.h"
#include "AssetRegistry/AssetDataTagMapSerializationDetails.h"
#include "EdGraph/EdGraphPin.h"

DEFINE_LOG_CATEGORY_STATIC(LogExecFlowGraph, Log, All);

#define LOCTEXT_NAMESPACE "BPExecFlowViewer"

// -----------------------------------------------------------------------
//  File-local helpers
// -----------------------------------------------------------------------
namespace
{
	/** Converts a normalised route label to a full-text output pin name. */
	FString RouteToCompactPinName(const FString& Route)
	{
		const FString Lower = Route.ToLower();
		if (Lower.Contains(TEXT("branch: true"))) return TEXT("True");
		if (Lower.Contains(TEXT("branch: false"))) return TEXT("False");
		if (Lower.Contains(TEXT("not valid"))) return TEXT("Not Valid");
		if (Lower.Contains(TEXT("is valid")) || Lower.Contains(TEXT("valid"))) return TEXT("Valid");
		// Plain exec -> caller uses "Out"
		return FString();
	}

	bool IsPrimaryRoute(const FString& RouteLabel)
	{
		const FString L = RouteLabel.ToLower();
		return L.Contains(TEXT("true")) ||
			(L.Contains(TEXT("valid")) && !L.Contains(TEXT("not")));
	}

	bool IsSecondaryRoute(const FString& RouteLabel)
	{
		const FString L = RouteLabel.ToLower();
		return L.Contains(TEXT("false")) || L.Contains(TEXT("not valid"));
	}

	int32 GetRoutePriority(const FString& RouteLabel)
	{
		if (IsPrimaryRoute(RouteLabel)) return 0;
		if (IsSecondaryRoute(RouteLabel)) return 1;
		return 2;
	}

	float ComputeMedianRank(TArray<int32>& InRanks)
	{
		if (InRanks.Num() == 0)
		{
			return TNumericLimits<float>::Max();
		}

		InRanks.Sort();
		const int32 Mid = InRanks.Num() / 2;
		if ((InRanks.Num() % 2) == 1)
		{
			return static_cast<float>(InRanks[Mid]);
		}

		return 0.5f * static_cast<float>(InRanks[Mid - 1] + InRanks[Mid]);
	}

	int32 FindNearestFreeLane(const int32 DesiredLane, TSet<int32>& UsedLanes)
	{
		if (!UsedLanes.Contains(DesiredLane))
		{
			UsedLanes.Add(DesiredLane);
			return DesiredLane;
		}

		for (int32 Offset = 1; Offset < 1024; ++Offset)
		{
			const int32 UpLane = DesiredLane - Offset;
			if (!UsedLanes.Contains(UpLane))
			{
				UsedLanes.Add(UpLane);
				return UpLane;
			}

			const int32 DownLane = DesiredLane + Offset;
			if (!UsedLanes.Contains(DownLane))
			{
				UsedLanes.Add(DownLane);
				return DownLane;
			}
		}

		UsedLanes.Add(DesiredLane);
		return DesiredLane;
	}

	/** Returns the output pin name for a given route label. */
	FString RouteToOutputPinName(const FString& Route)
	{
		const FString Compact = RouteToCompactPinName(Route);
		return Compact.IsEmpty() ? TEXT("Out") : Compact;
	}

	struct FNodeBounds
	{
		float Left = 0.0f;
		float Top = 0.0f;
		float Right = 0.0f;
		float Bottom = 0.0f;
	};

	static FLinearColor MakeClusterBaseColor(const FString& Key)
	{
		const uint32 Hash = GetTypeHash(Key);
		const float Hue = static_cast<float>(Hash % 360u);

		return FLinearColor::MakeFromHSV8(
			static_cast<uint8>(Hue / 360.0f * 255.0f),
			140, // saturation
			220 // value
		);
	}

	static void BuildClusterColors(const FString& Key, FLinearColor& OutFill, FLinearColor& OutBorder)
	{
		const FLinearColor Base = MakeClusterBaseColor(Key);
		OutFill = FLinearColor(Base.R, Base.G, Base.B, 0.12f);
		OutBorder = FLinearColor(Base.R, Base.G, Base.B, 0.60f);
	}

	static bool DoRectsOverlap(const FExecFlowClusterVisual& A, const FExecFlowClusterVisual& B)
	{
		return A.Min.X < B.Max.X &&
			A.Max.X > B.Min.X &&
			A.Min.Y < B.Max.Y &&
			A.Max.Y > B.Min.Y;
	}

	static bool HaveSameBlueprintLabel(const FExecFlowClusterVisual& A, const FExecFlowClusterVisual& B)
	{
		return A.Label == B.Label;
	}

	static void ShiftNodeX(UExecFlowGraphNode* Node, const float DeltaX)
	{
		if (!Node || FMath::IsNearlyZero(DeltaX))
		{
			return;
		}
		Node->NodePosX = FMath::RoundToInt(static_cast<float>(Node->NodePosX) + DeltaX);
	}

	static void PropagateShiftToDownstream(
		const TArray<UExecFlowGraphNode*>& SeedNodes,
		const TArray<FExecFlowEdge>& RenderEdges,
		const TArray<UExecFlowGraphNode*>& IndexToNode,
		const float DeltaX,
		TSet<UExecFlowGraphNode*>& InOutAlreadyShifted)
	{
		if (FMath::IsNearlyZero(DeltaX) || SeedNodes.Num() == 0)
		{
			return;
		}

		UE_LOG(LogExecFlowGraph, Log,
		       TEXT("PropagateShiftToDownstream: Seeds=%d DeltaX=%.1f Edges=%d"),
		       SeedNodes.Num(),
		       DeltaX,
		       RenderEdges.Num());

		TMap<int32, int32> PreShiftXByIndex;
		PreShiftXByIndex.Reserve(IndexToNode.Num());

		for (int32 Idx = 0; Idx < IndexToNode.Num(); ++Idx)
		{
			if (UExecFlowGraphNode* Node = IndexToNode[Idx])
			{
				PreShiftXByIndex.Add(Idx, Node->NodePosX);
			}
		}

		TArray<int32> QueueIndices;
		for (UExecFlowGraphNode* Seed : SeedNodes)
		{
			if (!Seed)
			{
				continue;
			}

			for (int32 Idx = 0; Idx < IndexToNode.Num(); ++Idx)
			{
				if (IndexToNode[Idx] == Seed)
				{
					QueueIndices.Add(Idx);
					break;
				}
			}
		}

		for (int32 Q = 0; Q < QueueIndices.Num(); ++Q)
		{
			const int32 CurIdx = QueueIndices[Q];
			if (!IndexToNode.IsValidIndex(CurIdx))
			{
				continue;
			}

			UExecFlowGraphNode* Cur = IndexToNode[CurIdx];
			if (!Cur)
			{
				continue;
			}

			const int32* CurPreShiftX = PreShiftXByIndex.Find(CurIdx);
			if (!CurPreShiftX)
			{
				continue;
			}

			for (const FExecFlowEdge& E : RenderEdges)
			{
				// Only follow OUTGOING edges from this node (downstream direction only).
				if (E.FromIdx != CurIdx)
				{
					continue;
				}

				const int32 NeighborIdx = E.ToIdx;

				if (!IndexToNode.IsValidIndex(NeighborIdx))
				{
					continue;
				}

				UExecFlowGraphNode* NeighborNode = IndexToNode[NeighborIdx];
				if (!NeighborNode)
				{
					continue;
				}

				const int32* NeighborPreShiftX = PreShiftXByIndex.Find(NeighborIdx);
				if (NeighborPreShiftX && *NeighborPreShiftX <= *CurPreShiftX)
				{
					continue;
				}

				if (!InOutAlreadyShifted.Contains(NeighborNode))
				{
					ShiftNodeX(NeighborNode, DeltaX);
					UE_LOG(LogExecFlowGraph, Log,
					       TEXT("  DownstreamShift: To='%s' NewX=%d"),
					       *NeighborNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
					       NeighborNode->NodePosX);
					InOutAlreadyShifted.Add(NeighborNode);
					QueueIndices.Add(NeighborIdx);
				}
			}
		}
	}

	static void ShiftClusterAndMembers(
		FExecFlowClusterVisual& Cluster,
		const float DeltaX,
		TMap<FString, TArray<UExecFlowGraphNode*>>& ClusterKeyToNodes,
		const TArray<FExecFlowEdge>& RenderEdges,
		const TArray<UExecFlowGraphNode*>& IndexToNode,
		TSet<UExecFlowGraphNode*>& InOutAlreadyShifted)
	{
		if (FMath::IsNearlyZero(DeltaX))
		{
			return;
		}

		Cluster.Min.X += DeltaX;
		Cluster.Max.X += DeltaX;

		TArray<UExecFlowGraphNode*>* Members = ClusterKeyToNodes.Find(Cluster.Key);
		if (!Members)
		{
			return;
		}

		// REMOVE the local TSet<UExecFlowGraphNode*> AlreadyShifted; — use InOutAlreadyShifted instead

		for (UExecFlowGraphNode* Node : *Members)
		{
			if (!Node)
			{
				continue;
			}
			if (!InOutAlreadyShifted.Contains(Node)) // <-- guard seeds too
			{
				ShiftNodeX(Node, DeltaX);
				InOutAlreadyShifted.Add(Node);
			}
		}

		PropagateShiftToDownstream(*Members, RenderEdges, IndexToNode, DeltaX, InOutAlreadyShifted);
	}

	static void ResolveInterBlueprintClusterOverlaps(
		TArray<FExecFlowClusterVisual>& InOutClusters,
		TMap<FString, TArray<UExecFlowGraphNode*>>& ClusterKeyToNodes,
		const TArray<FExecFlowEdge>& RenderEdges,
		const TArray<UExecFlowGraphNode*>& IndexToNode)
	{
		const float MinGapX = 64.0f;
		const int32 MaxPasses = 32;

		for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
		{
			bool bAnyMoved = false;
			TSet<UExecFlowGraphNode*> AlreadyShiftedThisPass;

			for (int32 i = 0; i < InOutClusters.Num(); ++i)
			{
				for (int32 j = i + 1; j < InOutClusters.Num(); ++j)
				{
					FExecFlowClusterVisual& A = InOutClusters[i];
					FExecFlowClusterVisual& B = InOutClusters[j];

					if (HaveSameBlueprintLabel(A, B))
					{
						continue;
					}

					UE_LOG(LogExecFlowGraph, Log,
					       TEXT("SolveCheck: A='%s' B='%s' A[(%.1f,%.1f)->(%.1f,%.1f)] B[(%.1f,%.1f)->(%.1f,%.1f)]"),
					       *A.Key, *B.Key,
					       A.Min.X, A.Min.Y, A.Max.X, A.Max.Y,
					       B.Min.X, B.Min.Y, B.Max.X, B.Max.Y);

					if (!DoRectsOverlap(A, B))
					{
						continue;
					}

					FExecFlowClusterVisual* MoveTarget = &B;
					const FExecFlowClusterVisual* Anchor = &A;

					if (A.Min.X > B.Min.X || (A.Min.X == B.Min.X && i > j))
					{
						MoveTarget = &A;
						Anchor = &B;
					}

					const float DesiredMinX = Anchor->Max.X + MinGapX;
					const float DeltaX = DesiredMinX - MoveTarget->Min.X;

					UE_LOG(LogExecFlowGraph, Log,
					       TEXT("SolveMove: Anchor='%s' Move='%s' DesiredMinX=%.1f DeltaX=%.1f"),
					       *Anchor->Key, *MoveTarget->Key, DesiredMinX, DeltaX);
					if (DeltaX > 0.0f)
					{
						ShiftClusterAndMembers(*MoveTarget, DeltaX, ClusterKeyToNodes, RenderEdges, IndexToNode,
						                       AlreadyShiftedThisPass);
						bAnyMoved = true;
					}
				}
			}

			if (!bAnyMoved)
			{
				break;
			}
		}
	}
} // namespace

// -----------------------------------------------------------------------
//  UExecFlowGraphSchema
// -----------------------------------------------------------------------

FLinearColor UExecFlowGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

const FPinConnectionResponse UExecFlowGraphSchema::CanCreateConnection(
	const UEdGraphPin* A, const UEdGraphPin* B) const
{
	return FPinConnectionResponse(
		CONNECT_RESPONSE_DISALLOW,
		LOCTEXT("ReadOnly", "Execution flow graph is read-only"));
}

// -----------------------------------------------------------------------
//  UExecFlowGraphNode
// -----------------------------------------------------------------------

void UExecFlowGraphNode::AllocateDefaultPins()
{
	// Single input exec pin (left edge)
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("In"));

	// Collect unique output pin names from all outgoing routes.
	TArray<FString> OutNames;
	for (const FExecFuncEntry& Func : GroupData.Functions)
	{
		for (const FString& Route : Func.OutgoingRouteLabels)
			OutNames.AddUnique(RouteToOutputPinName(Route));
	}

	if (OutNames.Num() == 0)
		OutNames.Add(TEXT("Out"));

	// Sort: "True" / "Valid" on top (first pin = uppermost in right rail)
	OutNames.Sort([](const FString& A, const FString& B)
	{
		auto IsPrimary = [](const FString& N) -> bool
		{
			return N == TEXT("True") || N == TEXT("Valid");
		};
		if (IsPrimary(A) != IsPrimary(B)) return IsPrimary(A);
		return A < B;
	});

	for (const FString& Name : OutNames)
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, *Name);
}

FText UExecFlowGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GroupData.BlueprintName);
}

FText UExecFlowGraphNode::GetTooltipText() const
{
	FString Tip = GroupData.BlueprintName + TEXT("\n");
	for (const FExecFuncEntry& F : GroupData.Functions)
	{
		Tip += TEXT("  ");
		switch (F.Kind)
		{
		case EExecNodeKind::Event: Tip += TEXT("[Event] ");
			break;
		case EExecNodeKind::CustomEvent: Tip += TEXT("[CustomEvent] ");
			break;
		case EExecNodeKind::ExecStep: Tip += TEXT("[Step] ");
			break;
		case EExecNodeKind::Macro: Tip += TEXT("[Macro] ");
			break;
		default: Tip += TEXT("[Function] ");
			break;
		}
		Tip += F.DisplayName.IsEmpty() ? F.FunctionName.ToString() : F.DisplayName;
		if (F.OutgoingRouteLabels.Num() > 0)
		{
			Tip += TEXT("  → ");
			Tip += FString::Join(F.OutgoingRouteLabels, TEXT(", "));
		}
		if (F.bIsCycleTruncated) Tip += TEXT(" (cycle)");
		if (!F.IntraGraphExecPath.IsEmpty())
		{
			Tip += TEXT("\n    Path: ");
			Tip += F.IntraGraphExecPath;
		}
		Tip += TEXT("\n");
	}
	return FText::FromString(Tip);
}

FLinearColor UExecFlowGraphNode::GetNodeTitleColor() const
{
	if (GroupData.DepthColumn < 0) return FLinearColor(0.30f, 0.55f, 1.00f); // blue  — callers
	if (GroupData.DepthColumn > 0) return FLinearColor(0.25f, 0.90f, 0.45f); // green — callees
	return FLinearColor(1.00f, 0.80f, 0.20f); // gold  — root
}

UEdGraphPin* UExecFlowGraphNode::GetInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
		if (Pin && Pin->Direction == EGPD_Input) return Pin;
	return nullptr;
}

UEdGraphPin* UExecFlowGraphNode::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
		if (Pin && Pin->Direction == EGPD_Output) return Pin;
	return nullptr;
}

UEdGraphPin* UExecFlowGraphNode::GetOutputPinForRoute(const FString& CompactRoute) const
{
	if (!CompactRoute.IsEmpty())
	{
		for (UEdGraphPin* Pin : Pins)
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString() == CompactRoute)
				return Pin;
	}
	return GetOutputPin(); // fallback to first output pin
}

// -----------------------------------------------------------------------
//  UExecFlowGraph
// -----------------------------------------------------------------------

UExecFlowGraph* UExecFlowGraph::Create(UObject* Outer)
{
	UExecFlowGraph* Graph = NewObject<UExecFlowGraph>(Outer);
	Graph->Schema = UExecFlowGraphSchema::StaticClass();
	return Graph;
}

bool UExecFlowGraph::PopulateGraph(UExecFlowGraph* Graph, const FExecFlowMap& FlowMap)
{
	if (!Graph)
	{
		UE_LOG(LogExecFlowGraph, Error, TEXT("PopulateGraph: Graph is null"));
		return false;
	}

	Graph->Nodes.Empty();
	Graph->ClusterVisuals.Reset();

	UE_LOG(LogExecFlowGraph, Log, TEXT("PopulateGraph: FlowMap has %d groups"), FlowMap.Groups.Num());

	if (FlowMap.Groups.Num() == 0)
	{
		UE_LOG(LogExecFlowGraph, Warning, TEXT("PopulateGraph: FlowMap has no groups — aborting"));
		return false;
	}

	// Expand to render groups: one visible graph node per function/exec-step entry.
	TArray<FExecBPGroup> RenderGroups;
	RenderGroups.Reserve(FlowMap.Groups.Num());
	TMap<int32, TArray<int32>> OriginalToRender;

	for (int32 GroupIdx = 0; GroupIdx < FlowMap.Groups.Num(); ++GroupIdx)
	{
		const FExecBPGroup& SourceGroup = FlowMap.Groups[GroupIdx];

		if (SourceGroup.Functions.Num() == 0)
		{
			const int32 RenderIdx = RenderGroups.Add(SourceGroup);
			OriginalToRender.FindOrAdd(GroupIdx).Add(RenderIdx);
			continue;
		}

		for (const FExecFuncEntry& Entry : SourceGroup.Functions)
		{
			FExecBPGroup SingleEntryGroup = SourceGroup;
			SingleEntryGroup.Functions.Reset();
			SingleEntryGroup.Functions.Add(Entry);
			SingleEntryGroup.bIsSynthetic = true;

			const int32 RenderIdx = RenderGroups.Add(MoveTemp(SingleEntryGroup));
			OriginalToRender.FindOrAdd(GroupIdx).Add(RenderIdx);
		}
	}

	// Build render edges from original edges and preserve local order within expanded groups.
	TArray<FExecFlowEdge> RenderEdges;
	for (const FExecFlowEdge& Edge : FlowMap.Edges)
	{
		const TArray<int32>* FromExpanded = OriginalToRender.Find(Edge.FromIdx);
		const TArray<int32>* ToExpanded = OriginalToRender.Find(Edge.ToIdx);
		if (!FromExpanded || !ToExpanded || FromExpanded->Num() == 0 || ToExpanded->Num() == 0)
			continue;

		RenderEdges.AddUnique(FExecFlowEdge{FromExpanded->Last(), ToExpanded->operator[](0), Edge.RouteLabel});
	}

	for (const TPair<int32, TArray<int32>>& Pair : OriginalToRender)
	{
		const TArray<int32>& Expanded = Pair.Value;
		for (int32 i = 0; i + 1 < Expanded.Num(); ++i)
			RenderEdges.AddUnique(FExecFlowEdge{Expanded[i], Expanded[i + 1], FString()});
	}

	// ---- Layout constants --------------------------------------------
	const float ColSpacing = 400.0f;
	const float GroupSpacing = 24.0f;
	const float HeaderHeight = 50.0f;
	const float FuncRowHeight = 26.0f;
	const float BodyPadding = 20.0f;
	const float MinNodeHeight = 80.0f;

	float MaxNodeHeight = MinNodeHeight;
	for (const FExecBPGroup& RG : RenderGroups)
	{
		const float H = FMath::Max(MinNodeHeight, HeaderHeight + RG.Functions.Num() * FuncRowHeight + BodyPadding);
		MaxNodeHeight = FMath::Max(MaxNodeHeight, H);
	}
	const float LaneSpacing = MaxNodeHeight + GroupSpacing;

	TMap<int32, TArray<int32>> ColumnGroups;
	for (int32 i = 0; i < RenderGroups.Num(); ++i)
		ColumnGroups.FindOrAdd(RenderGroups[i].DepthColumn).Add(i);

	// Deterministic per-column ordering:
	// 1) parent continuity from previous depth (median parent rank)
	// 2) route bias (True/Valid above False/Not Valid)
	// 3) original insertion order
	TArray<int32> SortedDepths;
	ColumnGroups.GetKeys(SortedDepths);
	SortedDepths.Sort();

	for (const int32 Depth : SortedDepths)
	{
		TArray<int32>& ColIndices = ColumnGroups.FindChecked(Depth);
		const TArray<int32>* PrevColIndices = ColumnGroups.Find(Depth - 1);
		if (!PrevColIndices || PrevColIndices->Num() == 0)
		{
			continue;
		}

		TMap<int32, int32> PrevOrder;
		for (int32 PrevRank = 0; PrevRank < PrevColIndices->Num(); ++PrevRank)
		{
			PrevOrder.Add((*PrevColIndices)[PrevRank], PrevRank);
		}

		struct FSortScore
		{
			bool bHasPrevParent = false;
			float ParentMedian = TNumericLimits<float>::Max();
			int32 RoutePriority = 2;
		};

		TMap<int32, FSortScore> Scores;
		for (const int32 NodeIdx : ColIndices)
		{
			TArray<int32> ParentRanks;
			int32 BestRoutePriority = 2;

			for (const FExecFlowEdge& E : RenderEdges)
			{
				if (E.ToIdx != NodeIdx)
				{
					continue;
				}

				const int32* ParentRank = PrevOrder.Find(E.FromIdx);
				if (!ParentRank)
				{
					continue;
				}

				ParentRanks.Add(*ParentRank);
				BestRoutePriority = FMath::Min(BestRoutePriority, GetRoutePriority(E.RouteLabel));
			}

			FSortScore Score;
			Score.bHasPrevParent = ParentRanks.Num() > 0;
			Score.ParentMedian = ComputeMedianRank(ParentRanks);
			Score.RoutePriority = BestRoutePriority;
			Scores.Add(NodeIdx, Score);
		}

		ColIndices.StableSort([&Scores](int32 A, int32 B)
		{
			const FSortScore& SA = Scores.FindChecked(A);
			const FSortScore& SB = Scores.FindChecked(B);

			if (SA.bHasPrevParent != SB.bHasPrevParent)
			{
				return SA.bHasPrevParent; // scored nodes first, dangling nodes later
			}
			if (SA.ParentMedian != SB.ParentMedian)
			{
				return SA.ParentMedian < SB.ParentMedian;
			}
			if (SA.RoutePriority != SB.RoutePriority)
			{
				return SA.RoutePriority < SB.RoutePriority;
			}
			return A < B;
		});
	}

	// Refine per-column ordering using crossing-minimization before lane assignment.
	// Process columns left-to-right, reorder each column by parent lane medians.
	for (int32 DepthIdx = 1; DepthIdx < SortedDepths.Num(); ++DepthIdx)
	{
		const int32 Depth = SortedDepths[DepthIdx];
		TArray<int32>& ColIndices = ColumnGroups.FindChecked(Depth);
		const TArray<int32>* PrevColIndices = ColumnGroups.Find(Depth - 1);

		if (!PrevColIndices || PrevColIndices->Num() == 0)
		{
			continue;
		}

		TMap<int32, float> NodeParentMedianLane;
		for (const int32 NodeIdx : ColIndices)
		{
			TArray<float> ParentLanesFloat;

			for (const FExecFlowEdge& E : RenderEdges)
			{
				if (E.ToIdx != NodeIdx)
				{
					continue;
				}

				const int32* PrevIdx = PrevColIndices->FindByPredicate([&](int32 X) { return X == E.FromIdx; });
				if (!PrevIdx)
				{
					continue;
				}

				const int32 PrevRank = PrevColIndices->Find(E.FromIdx);
				const int32 StartLanePrev = -((PrevColIndices->Num() - 1) / 2);
				const float ParentLane = static_cast<float>(StartLanePrev + PrevRank);
				ParentLanesFloat.Add(ParentLane);
			}

			float Median = TNumericLimits<float>::Max();
			if (ParentLanesFloat.Num() > 0)
			{
				ParentLanesFloat.Sort();
				const int32 Mid = ParentLanesFloat.Num() / 2;
				if ((ParentLanesFloat.Num() % 2) == 1)
				{
					Median = ParentLanesFloat[Mid];
				}
				else
				{
					Median = 0.5f * (ParentLanesFloat[Mid - 1] + ParentLanesFloat[Mid]);
				}
			}

			NodeParentMedianLane.Add(NodeIdx, Median);
		}

		ColIndices.StableSort([&NodeParentMedianLane](int32 A, int32 B)
		{
			const float MedianA = NodeParentMedianLane.FindRef(A);
			const float MedianB = NodeParentMedianLane.FindRef(B);

			const bool bAHasParent = MedianA != TNumericLimits<float>::Max();
			const bool bBHasParent = MedianB != TNumericLimits<float>::Max();

			if (bAHasParent != bBHasParent)
			{
				return bAHasParent;
			}
			if (MedianA != MedianB)
			{
				return MedianA < MedianB;
			}

			return A < B;
		});
	}

	// Lane assignment keeps related nodes on shared horizontal tracks.
	TMap<int32, int32> NodeToLane;
	for (const int32 Depth : SortedDepths)
	{
		const TArray<int32>& ColIndices = ColumnGroups.FindChecked(Depth);
		TSet<int32> UsedLanes;
		const TArray<int32>* PrevColIndices = ColumnGroups.Find(Depth - 1);

		if (!PrevColIndices || PrevColIndices->Num() == 0)
		{
			const int32 StartLane = -((ColIndices.Num() - 1) / 2);
			for (int32 i = 0; i < ColIndices.Num(); ++i)
			{
				const int32 Lane = FindNearestFreeLane(StartLane + i, UsedLanes);
				NodeToLane.Add(ColIndices[i], Lane);
			}
			continue;
		}

		const int32 StartLane = -((ColIndices.Num() - 1) / 2);
		for (int32 i = 0; i < ColIndices.Num(); ++i)
		{
			const int32 NodeIdx = ColIndices[i];
			TArray<int32> ParentLanes;

			for (const FExecFlowEdge& E : RenderEdges)
			{
				if (E.ToIdx != NodeIdx)
				{
					continue;
				}

				if (const int32* ParentLane = NodeToLane.Find(E.FromIdx))
				{
					ParentLanes.Add(*ParentLane);
				}
			}

			int32 DesiredLane = StartLane + i;
			if (ParentLanes.Num() > 0)
			{
				ParentLanes.Sort();
				const int32 Mid = ParentLanes.Num() / 2;
				if ((ParentLanes.Num() % 2) == 1)
				{
					DesiredLane = ParentLanes[Mid];
				}
				else
				{
					DesiredLane = FMath::RoundToInt(0.5f * static_cast<float>(ParentLanes[Mid - 1] + ParentLanes[Mid]));
				}
			}

			const int32 Lane = FindNearestFreeLane(DesiredLane, UsedLanes);
			NodeToLane.Add(NodeIdx, Lane);
		}
	}

	TArray<UExecFlowGraphNode*> IndexToNode;
	IndexToNode.Init(nullptr, RenderGroups.Num());
	TMap<int32, FNodeBounds> RenderNodeBoundsByIndex;
	const float NodeWidthEstimate = 280.0f;

	for (auto& KV : ColumnGroups)
	{
		const int32 DepthCol = KV.Key;
		const TArray<int32>& GroupIndices = KV.Value;
		const float X = static_cast<float>(DepthCol) * ColSpacing;

		for (int32 GIdx : GroupIndices)
		{
			const float H = FMath::Max(MinNodeHeight,
			                           HeaderHeight + RenderGroups[GIdx].Functions.Num() * FuncRowHeight + BodyPadding);
			const int32 Lane = NodeToLane.FindRef(GIdx);
			const float CenterY = static_cast<float>(Lane) * LaneSpacing;

			UExecFlowGraphNode* NewNode = NewObject<UExecFlowGraphNode>(Graph);
			NewNode->GroupData = RenderGroups[GIdx];
			NewNode->NodePosX = FMath::RoundToInt(X);
			NewNode->NodePosY = FMath::RoundToInt(CenterY - (0.5f * H));
			const float NodeLeft = static_cast<float>(NewNode->NodePosX);
			const float NodeTop = static_cast<float>(NewNode->NodePosY);
			const float NodeRight = NodeLeft + NodeWidthEstimate;
			const float NodeBottom = NodeTop + H;

			RenderNodeBoundsByIndex.Add(GIdx, FNodeBounds{NodeLeft, NodeTop, NodeRight, NodeBottom});

			NewNode->AllocateDefaultPins();
			Graph->AddNode(NewNode, false, false);

			IndexToNode[GIdx] = NewNode;
		}
	}

	const float ClusterPadX = 36.0f;
	const float ClusterPadY = 28.0f;
	const float ClusterSplitGap = ColSpacing * 1.25f;

	TMap<FString, TArray<int32>> ClusterToRenderIndices;

	for (int32 RenderIdx = 0; RenderIdx < RenderGroups.Num(); ++RenderIdx)
	{
		const FExecBPGroup& RG = RenderGroups[RenderIdx];
		const FString ClusterKey = RG.ClusterBlueprintName.IsEmpty()
			                           ? RG.BlueprintName
			                           : RG.ClusterBlueprintName;
		ClusterToRenderIndices.FindOrAdd(ClusterKey).Add(RenderIdx);
	}

	UE_LOG(LogExecFlowGraph, Log, TEXT("PopulateGraph: Built %d blueprint buckets from %d render groups"),
	       ClusterToRenderIndices.Num(), RenderGroups.Num());

	TMap<FString, TArray<UExecFlowGraphNode*>> ClusterKeyToNodes;

	for (const TPair<FString, TArray<int32>>& Pair : ClusterToRenderIndices)
	{
		const FString& ClusterKey = Pair.Key;
		TArray<int32> MemberIndices = Pair.Value;

		if (MemberIndices.Num() == 0)
		{
			continue;
		}

		MemberIndices.Sort([&RenderNodeBoundsByIndex](const int32 A, const int32 B)
		{
			const FNodeBounds* BoundsA = RenderNodeBoundsByIndex.Find(A);
			const FNodeBounds* BoundsB = RenderNodeBoundsByIndex.Find(B);
			if (!BoundsA || !BoundsB)
			{
				return A < B;
			}
			return BoundsA->Left < BoundsB->Left;
		});

		TArray<TArray<int32>> Segments;
		for (const int32 RenderIdx : MemberIndices)
		{
			const FNodeBounds* Bounds = RenderNodeBoundsByIndex.Find(RenderIdx);
			if (!Bounds)
			{
				UE_LOG(LogExecFlowGraph, Warning,
				       TEXT("PopulateGraph: No bounds found for render index %d in blueprint '%s'"), RenderIdx,
				       *ClusterKey);
				continue;
			}

			if (Segments.Num() == 0)
			{
				Segments.AddDefaulted();
				Segments.Last().Add(RenderIdx);
				continue;
			}

			TArray<int32>& LastSegment = Segments.Last();
			const int32 LastIdx = LastSegment.Last();
			const FNodeBounds* LastBounds = RenderNodeBoundsByIndex.Find(LastIdx);
			if (!LastBounds)
			{
				LastSegment.Add(RenderIdx);
				continue;
			}

			const float GapX = Bounds->Left - LastBounds->Right;
			if (GapX > ClusterSplitGap)
			{
				Segments.AddDefaulted();
				Segments.Last().Add(RenderIdx);
			}
			else
			{
				LastSegment.Add(RenderIdx);
			}
		}

		UE_LOG(LogExecFlowGraph, Log, TEXT("PopulateGraph: Blueprint '%s' split into %d segment(s)"), *ClusterKey,
		       Segments.Num());

		for (int32 SegmentIdx = 0; SegmentIdx < Segments.Num(); ++SegmentIdx)
		{
			const TArray<int32>& SegmentMembers = Segments[SegmentIdx];
			if (SegmentMembers.Num() == 0)
			{
				continue;
			}

			bool bHasAnyBounds = false;
			FVector2D Min(FLT_MAX, FLT_MAX);
			FVector2D Max(-FLT_MAX, -FLT_MAX);

			for (const int32 RenderIdx : SegmentMembers)
			{
				const FNodeBounds* Bounds = RenderNodeBoundsByIndex.Find(RenderIdx);
				if (!Bounds)
				{
					continue;
				}

				bHasAnyBounds = true;
				Min.X = FMath::Min(Min.X, Bounds->Left);
				Min.Y = FMath::Min(Min.Y, Bounds->Top);
				Max.X = FMath::Max(Max.X, Bounds->Right);
				Max.Y = FMath::Max(Max.Y, Bounds->Bottom);
			}

			if (!bHasAnyBounds)
			{
				UE_LOG(LogExecFlowGraph, Warning,
				       TEXT("PopulateGraph: Blueprint '%s' segment %d had no valid bounds — skipped"), *ClusterKey,
				       SegmentIdx);
				continue;
			}

			Min.X -= ClusterPadX;
			Min.Y -= ClusterPadY;
			Max.X += ClusterPadX;
			Max.Y += ClusterPadY;

			FExecFlowClusterVisual Visual;
			Visual.Key = FString::Printf(TEXT("%s#%d"), *ClusterKey, SegmentIdx);
			Visual.Label = ClusterKey;
			Visual.Min = Min;
			Visual.Max = Max;
			BuildClusterColors(ClusterKey, Visual.FillColor, Visual.BorderColor);

			UE_LOG(LogExecFlowGraph, Log,
			       TEXT("PopulateGraph: Cluster '%s' (segment %d) — Min=(%.1f, %.1f) Max=(%.1f, %.1f) Members=%d"),
			       *ClusterKey, SegmentIdx, Min.X, Min.Y, Max.X, Max.Y, SegmentMembers.Num());

			// Capture node membership for later overlap-driven node shifts.
			TArray<UExecFlowGraphNode*>& ClusterMembers = ClusterKeyToNodes.FindOrAdd(Visual.Key);
			for (const int32 RenderIdx : SegmentMembers)
			{
				if (IndexToNode.IsValidIndex(RenderIdx) && IndexToNode[RenderIdx])
				{
					ClusterMembers.Add(IndexToNode[RenderIdx]);
				}
			}

			Graph->ClusterVisuals.Add(MoveTemp(Visual));
		}
	}

	UE_LOG(LogExecFlowGraph, Log, TEXT("---- PreSolve Cluster Bounds ----"));
	for (const FExecFlowClusterVisual& V : Graph->ClusterVisuals)
	{
		UE_LOG(LogExecFlowGraph, Log,
		       TEXT("PreSolve: Key='%s' Label='%s' Min=(%.1f, %.1f) Max=(%.1f, %.1f)"),
		       *V.Key, *V.Label, V.Min.X, V.Min.Y, V.Max.X, V.Max.Y);
	}

	ResolveInterBlueprintClusterOverlaps(Graph->ClusterVisuals, ClusterKeyToNodes, RenderEdges, IndexToNode);

	// Re-fit cluster visuals from current member node positions after overlap shifts.
	for (FExecFlowClusterVisual& Visual : Graph->ClusterVisuals)
	{
		const TArray<UExecFlowGraphNode*>* Members = ClusterKeyToNodes.Find(Visual.Key);
		if (!Members || Members->Num() == 0)
		{
			continue;
		}

		bool bHasAny = false;
		FVector2D Min(FLT_MAX, FLT_MAX);
		FVector2D Max(-FLT_MAX, -FLT_MAX);

		for (UExecFlowGraphNode* Node : *Members)
		{
			if (!Node)
			{
				continue;
			}

			// Mirror the same node bounds model used during initial cluster build.
			const float NodeLeft = static_cast<float>(Node->NodePosX);
			const float NodeTop = static_cast<float>(Node->NodePosY);
			const float NodeHeight = FMath::Max(
				MinNodeHeight,
				HeaderHeight + Node->GroupData.Functions.Num() * FuncRowHeight + BodyPadding);
			const float NodeRight = NodeLeft + NodeWidthEstimate;
			const float NodeBottom = NodeTop + NodeHeight;

			bHasAny = true;
			Min.X = FMath::Min(Min.X, NodeLeft);
			Min.Y = FMath::Min(Min.Y, NodeTop);
			Max.X = FMath::Max(Max.X, NodeRight);
			Max.Y = FMath::Max(Max.Y, NodeBottom);
		}

		if (!bHasAny)
		{
			continue;
		}

		Visual.Min = FVector2D(Min.X - ClusterPadX, Min.Y - ClusterPadY);
		Visual.Max = FVector2D(Max.X + ClusterPadX, Max.Y + ClusterPadY);

		UE_LOG(LogExecFlowGraph, Log,
		       TEXT("Refit1: Key='%s' Members=%d Min=(%.1f, %.1f) Max=(%.1f, %.1f)"),
		       *Visual.Key, Members->Num(), Visual.Min.X, Visual.Min.Y, Visual.Max.X, Visual.Max.Y);

		UE_LOG(LogExecFlowGraph, Verbose,
		       TEXT("PopulateGraph: Refit cluster '%s' -> Min=(%.1f, %.1f) Max=(%.1f, %.1f) Members=%d"),
		       *Visual.Key, Visual.Min.X, Visual.Min.Y, Visual.Max.X, Visual.Max.Y, Members->Num());
	}

	// Final separation pass: prevent cross-blueprint cluster overlap after refit.
	// const float FinalGapX = 64.0f;
	// const int32 FinalMaxPasses = 16;
	//
	// for (int32 Pass = 0; Pass < FinalMaxPasses; ++Pass)
	// {
	// 	bool bMoved = false;
	//
	// 	for (int32 i = 0; i < Graph->ClusterVisuals.Num(); ++i)
	// 	{
	// 		for (int32 j = i + 1; j < Graph->ClusterVisuals.Num(); ++j)
	// 		{
	// 			FExecFlowClusterVisual& A = Graph->ClusterVisuals[i];
	// 			FExecFlowClusterVisual& B = Graph->ClusterVisuals[j];
	//
	// 			// Only enforce separation across different blueprints.
	// 			if (A.Label == B.Label)
	// 			{
	// 				continue;
	// 			}
	//
	// 			const bool bOverlap =
	// 				A.Min.X < B.Max.X && A.Max.X > B.Min.X &&
	// 				A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
	//
	// 			UE_LOG(LogExecFlowGraph, Log,
	// 			       TEXT("FinalSepCheck[P%d]: A='%s' B='%s' Overlap=%s"),
	// 			       Pass, *A.Key, *B.Key, bOverlap ? TEXT("YES") : TEXT("NO"));
	//
	// 			if (!bOverlap)
	// 			{
	// 				continue;
	// 			}
	//
	// 			// Move the right-side cluster further right.
	// 			FExecFlowClusterVisual* Move = &B;
	// 			FExecFlowClusterVisual* Anchor = &A;
	// 			if (A.Min.X > B.Min.X)
	// 			{
	// 				Move = &A;
	// 				Anchor = &B;
	// 			}
	//
	//
	// 			const float DesiredMinX = Anchor->Max.X + FinalGapX;
	// 			const float DeltaX = DesiredMinX - Move->Min.X;
	// 			if (DeltaX <= 0.0f)
	// 			{
	// 				continue;
	// 			}
	//
	// 			Move->Min.X += DeltaX;
	// 			Move->Max.X += DeltaX;
	//
	// 			UE_LOG(LogExecFlowGraph, Log,
	// 			       TEXT("FinalSepMove[P%d]: Anchor='%s' Move='%s' DeltaX=%.1f"),
	// 			       Pass, *Anchor->Key, *Move->Key, DeltaX);
	//
	// 			// Move member nodes with the shifted cluster.
	// 			if (TArray<UExecFlowGraphNode*>* Members = ClusterKeyToNodes.Find(Move->Key))
	// 			{
	// 				for (UExecFlowGraphNode* Node : *Members)
	// 				{
	// 					if (Node)
	// 					{
	// 						Node->NodePosX = FMath::RoundToInt(static_cast<float>(Node->NodePosX) + DeltaX);
	// 					}
	// 				}
	// 			}
	//
	// 			bMoved = true;
	// 		}
	// 	}
	//
	// 	if (!bMoved)
	// 	{
	// 		break;
	// 	}
	// }
	//
	// // Refit once more after final separation node moves.
	// for (FExecFlowClusterVisual& Visual : Graph->ClusterVisuals)
	// {
	// 	const TArray<UExecFlowGraphNode*>* Members = ClusterKeyToNodes.Find(Visual.Key);
	// 	if (!Members || Members->Num() == 0) continue;
	//
	// 	bool bHasAny = false;
	// 	FVector2D Min(FLT_MAX, FLT_MAX);
	// 	FVector2D Max(-FLT_MAX, -FLT_MAX);
	//
	// 	for (UExecFlowGraphNode* Node : *Members)
	// 	{
	// 		if (!Node) continue;
	//
	// 		const float NodeLeft = static_cast<float>(Node->NodePosX);
	// 		const float NodeTop = static_cast<float>(Node->NodePosY);
	// 		const float NodeHeight = FMath::Max(
	// 			MinNodeHeight,
	// 			HeaderHeight + Node->GroupData.Functions.Num() * FuncRowHeight + BodyPadding);
	// 		const float NodeRight = NodeLeft + NodeWidthEstimate;
	// 		const float NodeBottom = NodeTop + NodeHeight;
	//
	// 		bHasAny = true;
	// 		Min.X = FMath::Min(Min.X, NodeLeft);
	// 		Min.Y = FMath::Min(Min.Y, NodeTop);
	// 		Max.X = FMath::Max(Max.X, NodeRight);
	// 		Max.Y = FMath::Max(Max.Y, NodeBottom);
	// 	}
	//
	// 	if (!bHasAny) continue;
	//
	// 	Visual.Min = FVector2D(Min.X - ClusterPadX, Min.Y - ClusterPadY);
	// 	Visual.Max = FVector2D(Max.X + ClusterPadX, Max.Y + ClusterPadY);
	// 	UE_LOG(LogExecFlowGraph, Log,
	// 	       TEXT("Refit2: Key='%s' Members=%d Min=(%.1f, %.1f) Max=(%.1f, %.1f)"),
	// 	       *Visual.Key, Members->Num(), Visual.Min.X, Visual.Min.Y, Visual.Max.X, Visual.Max.Y);
	// }
	//
	// UE_LOG(LogExecFlowGraph, Log, TEXT("PopulateGraph: Final ClusterVisuals count = %d"), Graph->ClusterVisuals.Num());

	for (const FExecFlowClusterVisual& Visual : Graph->ClusterVisuals)
	{
		UE_LOG(LogExecFlowGraph, Verbose,
		       TEXT("PopulateGraph: Post-resolve cluster '%s' label='%s' Min=(%.1f, %.1f) Max=(%.1f, %.1f)"),
		       *Visual.Key, *Visual.Label, Visual.Min.X, Visual.Min.Y, Visual.Max.X, Visual.Max.Y);
	}

	for (const FExecFlowEdge& Edge : RenderEdges)
	{
		const bool bValidFrom = Edge.FromIdx >= 0 && Edge.FromIdx < IndexToNode.Num();
		const bool bValidTo = Edge.ToIdx >= 0 && Edge.ToIdx < IndexToNode.Num();
		if (!bValidFrom || !bValidTo) continue;

		UExecFlowGraphNode* FromNode = IndexToNode[Edge.FromIdx];
		UExecFlowGraphNode* ToNode = IndexToNode[Edge.ToIdx];
		if (!FromNode || !ToNode) continue;
		
		if (FromNode->NodePosX >= ToNode->NodePosX) continue;

		// Resolve the correct output pin by route label
		const FString PinName = RouteToOutputPinName(Edge.RouteLabel);
		UEdGraphPin* OutPin = FromNode->GetOutputPinForRoute(PinName);
		UEdGraphPin* InPin = ToNode->GetInputPin();
		if (OutPin && InPin)
			OutPin->MakeLinkTo(InPin);
	}

	// Log final exec flow
	UE_LOG(LogExecFlowGraph, Log, TEXT("---- Final Exec Flow ----"));
	for (const FExecFlowEdge& Edge : RenderEdges)
	{
		const bool bValidFrom = Edge.FromIdx >= 0 && Edge.FromIdx < IndexToNode.Num();
		const bool bValidTo = Edge.ToIdx >= 0 && Edge.ToIdx < IndexToNode.Num();
		if (!bValidFrom || !bValidTo) continue;

		const UExecFlowGraphNode* From = IndexToNode[Edge.FromIdx];
		const UExecFlowGraphNode* To = IndexToNode[Edge.ToIdx];
		if (!From || !To) continue;

		const FString FromFunc = From->GroupData.Functions.Num() > 0
			                         ? From->GroupData.Functions[0].DisplayName.IsEmpty()
				                           ? From->GroupData.Functions[0].FunctionName.ToString()
				                           : From->GroupData.Functions[0].DisplayName
			                         : TEXT("?");
		const FString ToFunc = To->GroupData.Functions.Num() > 0
			                       ? To->GroupData.Functions[0].DisplayName.IsEmpty()
				                         ? To->GroupData.Functions[0].FunctionName.ToString()
				                         : To->GroupData.Functions[0].DisplayName
			                       : TEXT("?");

		UE_LOG(LogExecFlowGraph, Log,
		       TEXT("  ExecFlow: [%s::%s] --%s--> [%s::%s]  (X:%d -> X:%d)"),
		       *From->GroupData.BlueprintName, *FromFunc,
		       Edge.RouteLabel.IsEmpty() ? TEXT("exec") : *Edge.RouteLabel,
		       *To->GroupData.BlueprintName, *ToFunc,
		       From->NodePosX, To->NodePosX);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
