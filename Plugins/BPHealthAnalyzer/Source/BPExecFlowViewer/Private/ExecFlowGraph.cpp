#include "ExecFlowGraph.h"

#include "K2Node_FunctionEntry.h"
#include "EdGraph/EdGraphPin.h"

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
	if (!Graph) return false;

	Graph->Nodes.Empty();
	if (FlowMap.Groups.Num() == 0) return false;

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
			NewNode->AllocateDefaultPins();
			Graph->AddNode(NewNode, false, false);

			IndexToNode[GIdx] = NewNode;
		}
	}

	for (const FExecFlowEdge& Edge : RenderEdges)
	{
		const bool bValidFrom = Edge.FromIdx >= 0 && Edge.FromIdx < IndexToNode.Num();
		const bool bValidTo = Edge.ToIdx >= 0 && Edge.ToIdx < IndexToNode.Num();
		if (!bValidFrom || !bValidTo) continue;

		UExecFlowGraphNode* FromNode = IndexToNode[Edge.FromIdx];
		UExecFlowGraphNode* ToNode = IndexToNode[Edge.ToIdx];
		if (!FromNode || !ToNode) continue;

		// Resolve the correct output pin by route label
		const FString PinName = RouteToOutputPinName(Edge.RouteLabel);
		UEdGraphPin* OutPin = FromNode->GetOutputPinForRoute(PinName);
		UEdGraphPin* InPin = ToNode->GetInputPin();
		if (OutPin && InPin)
			OutPin->MakeLinkTo(InPin);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
