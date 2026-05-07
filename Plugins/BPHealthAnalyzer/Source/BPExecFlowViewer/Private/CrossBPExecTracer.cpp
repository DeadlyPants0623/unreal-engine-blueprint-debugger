#include "CrossBPExecTracer.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"

DEFINE_LOG_CATEGORY_STATIC(LogBPExecFlowTracer, Log, All);

namespace
{
	struct FNodeMeta
	{
		int32 Depth = 0;
	};

	struct FTraversalItem
	{
		UEdGraphNode* Node = nullptr;
		int32 Depth = 0;
		UBlueprint* ContextBlueprint = nullptr;
	};

	struct FEdgeRecord
	{
		UEdGraphNode* From = nullptr;
		UEdGraphNode* To = nullptr;
		FString Route;
	};

	bool IsExecPin(const UEdGraphPin* Pin, EEdGraphPinDirection Dir)
	{
		return Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	FString NormalizeRouteLabel(const FString& InLabel)
	{
		if (InLabel.IsEmpty()) return FString();

		const FString Lower = InLabel.ToLower();
		if (Lower == TEXT("true")) return TEXT("Branch: True");
		if (Lower == TEXT("false")) return TEXT("Branch: False");
		if (Lower == TEXT("else")) return TEXT("Branch: False"); // UK2Node_IfThenElse PN_Else
		if (Lower.Contains(TEXT("is not valid")) || Lower.Contains(TEXT("not valid")))
			return
				TEXT("IsValid: Not Valid");
		if (Lower.Contains(TEXT("is valid"))) return TEXT("IsValid: Valid");
		if (Lower == TEXT("then") || Lower == TEXT("exec")) return TEXT("Exec");
		return InLabel;
	}

	FString NodeDisplayName(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("<null>");
		}
		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		return Title.IsEmpty() ? Node->GetClass()->GetName() : Title;
	}

	EExecNodeKind GetKind(const UEdGraphNode* Node)
	{
		if (!Node) return EExecNodeKind::ExecStep;
		if (Node->IsA<UK2Node_Event>()) return EExecNodeKind::Event;
		if (Node->IsA<UK2Node_CustomEvent>()) return EExecNodeKind::CustomEvent;
		if (Node->IsA<UK2Node_FunctionEntry>()) return EExecNodeKind::Function;
		return EExecNodeKind::ExecStep;
	}

	void AddOrUpdateNodeMeta(
		TMap<UEdGraphNode*, FNodeMeta>& NodeMeta,
		UEdGraphNode* Node,
		int32 Depth)
	{
		if (!Node)
		{
			return;
		}

		if (FNodeMeta* Existing = NodeMeta.Find(Node))
		{
			if (FMath::Abs(Depth) < FMath::Abs(Existing->Depth))
			{
				Existing->Depth = Depth;
			}
			return;
		}

		FNodeMeta Meta;
		Meta.Depth = Depth;
		NodeMeta.Add(Node, MoveTemp(Meta));
	}

	void AddEdgeUnique(TArray<FEdgeRecord>& Edges, TSet<FString>& EdgeKeys, UEdGraphNode* From, UEdGraphNode* To,
	                   const FString& Route)
	{
		if (!From || !To || From == To)
		{
			return;
		}

		const FString Key = FString::Printf(TEXT("%p|%p|%s"), From, To, *Route);
		if (EdgeKeys.Contains(Key))
		{
			return;
		}

		EdgeKeys.Add(Key);
		FEdgeRecord Edge;
		Edge.From = From;
		Edge.To = To;
		Edge.Route = Route;
		Edges.Add(MoveTemp(Edge));
	}

	UBlueprint* GetBlueprintFromClass(const UClass* InClass)
	{
		if (!InClass)
		{
			return nullptr;
		}

		return Cast<UBlueprint>(InClass->ClassGeneratedBy);
	}

	const UClass* ResolvePinOwnerClass(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return nullptr;
		}

		if (const UObject* PinTypeObject = Pin->PinType.PinSubCategoryObject.Get())
		{
			if (const UClass* TypedClass = Cast<UClass>(PinTypeObject))
			{
				return TypedClass;
			}
		}

		if (const UObject* DefaultObject = Pin->DefaultObject)
		{
			return DefaultObject->GetClass();
		}

		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}

			if (const UObject* LinkedTypeObject = LinkedPin->PinType.PinSubCategoryObject.Get())
			{
				if (const UClass* LinkedClass = Cast<UClass>(LinkedTypeObject))
				{
					return LinkedClass;
				}
			}

			if (const UObject* LinkedDefaultObject = LinkedPin->DefaultObject)
			{
				return LinkedDefaultObject->GetClass();
			}
		}

		return nullptr;
	}

	FString ResolveNodeOwnerLabel(const UEdGraphNode* Node, UBlueprint* DefaultBlueprint,
	                              UBlueprint*& OutOwnerBlueprint)
	{
		OutOwnerBlueprint = nullptr;
		if (!Node)
		{
			OutOwnerBlueprint = DefaultBlueprint;
			return DefaultBlueprint ? DefaultBlueprint->GetName() : TEXT("UnknownBP");
		}

		if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			const UEdGraphPin* SelfPin = CallFunctionNode->FindPin(UEdGraphSchema_K2::PN_Self);
			const UEdGraphPin* TargetPin = SelfPin ? SelfPin : CallFunctionNode->FindPin(TEXT("Target"));
			if (const UClass* TargetClass = ResolvePinOwnerClass(TargetPin))
			{
				if (UBlueprint* TargetBlueprint = GetBlueprintFromClass(TargetClass))
				{
					OutOwnerBlueprint = TargetBlueprint;
					return TargetBlueprint->GetName();
				}

				// return TargetClass->GetName();
			}

			if (const UFunction* TargetFunction = CallFunctionNode->GetTargetFunction())
			{
				if (const UClass* OwnerClass = TargetFunction->GetOwnerClass())
				{
					if (UBlueprint* OwnerBlueprint = GetBlueprintFromClass(OwnerClass))
					{
						OutOwnerBlueprint = OwnerBlueprint;
						return OwnerBlueprint->GetName();
					}

					// Native/non-blueprint owners still get an accurate title.
					// return OwnerClass->GetName();
				}
			}
		}

		if (const UEdGraph* NodeGraph = Node->GetGraph())
		{
			if (UBlueprint* GraphBlueprint = NodeGraph->GetTypedOuter<UBlueprint>())
			{
				OutOwnerBlueprint = GraphBlueprint;
				return GraphBlueprint->GetName();
			}
		}

		OutOwnerBlueprint = DefaultBlueprint;
		return DefaultBlueprint ? DefaultBlueprint->GetName() : TEXT("UnknownBP");
	}

	UEdGraphNode* FindCallableEntryInBlueprint(UBlueprint* BP, FName FunctionName)
	{
		if (!BP || FunctionName.IsNone())
		{
			return nullptr;
		}

		// 1) Normal function graphs
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (!Graph)
			{
				continue;
			}

			if (Graph->GetFName() == FunctionName)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
					{
						return Entry;
					}
				}
			}
		}

		// 2) Event graph custom events / events
		for (UEdGraph* Graph : BP->UbergraphPages)
		{
			if (!Graph)
			{
				continue;
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
				{
					if (CustomEvent->CustomFunctionName == FunctionName)
					{
						return const_cast<UK2Node_CustomEvent*>(CustomEvent);
					}
				}

				if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
				{
					if (EventNode->GetFunctionName() == FunctionName)
					{
						return const_cast<UK2Node_Event*>(EventNode);
					}
				}
			}
		}

		return nullptr;
	}
} // namespace

FExecFlowMap FCrossBPExecTracer::TraceFromNode(UEdGraphNode* SelectedNode, int32 BackwardDepth, int32 ForwardDepth)
{
	FExecFlowMap Result;
	if (!SelectedNode || !SelectedNode->GetGraph())
	{
		UE_LOG(LogBPExecFlowTracer, Warning,
		       TEXT("TraceFromNode aborted: invalid SelectedNode=%p (Graph=%p)"),
		       SelectedNode,
		       SelectedNode ? SelectedNode->GetGraph() : nullptr);
		return Result;
	}

	UEdGraph* RootGraph = SelectedNode->GetGraph();
	UBlueprint* OwnerBP = RootGraph->GetTypedOuter<UBlueprint>();

	UE_LOG(LogBPExecFlowTracer, Log,
	       TEXT("TraceFromNode start: Node='%s' Class='%s' Graph='%s' OwnerBP='%s' BackwardDepth=%d ForwardDepth=%d"),
	       *NodeDisplayName(SelectedNode),
	       *SelectedNode->GetClass()->GetName(),
	       *GetNameSafe(RootGraph),
	       *GetNameSafe(OwnerBP),
	       BackwardDepth,
	       ForwardDepth);

	TMap<UEdGraphNode*, FNodeMeta> NodeMeta;
	TArray<FEdgeRecord> Edges;
	TSet<FString> EdgeKeys;
	TMap<UEdGraphNode*, UBlueprint*> NodeCallerBlueprint;

	int32 ForwardQueueProcessed = 0;
	int32 ForwardDepthLimitedSkips = 0;
	int32 ForwardSameGraphEdgesAdded = 0;
	int32 ForwardSameGraphEnqueued = 0;
	int32 ForwardCrossBPAttempts = 0;
	int32 ForwardCrossBPVisitedSkips = 0;
	int32 ForwardCrossBPNoTargetBPSkips = 0;
	int32 ForwardCrossBPNoEntrySkips = 0;
	int32 ForwardCrossBPSuccesses = 0;

	int32 BackwardQueueProcessed = 0;
	int32 BackwardDepthLimitedSkips = 0;
	int32 BackwardCrossGraphSkips = 0;
	int32 BackwardEdgesAdded = 0;
	int32 BackwardEnqueued = 0;

	AddOrUpdateNodeMeta(NodeMeta, SelectedNode, 0);

	// Downstream traversal: current -> next
	if (ForwardDepth > 0)
	{
		TArray<FTraversalItem> Queue;
		TMap<UEdGraphNode*, int32> BestDepth;
		TSet<UEdGraphNode*> VisitedEntryNodes;

		// FIX 1: pass OwnerBP as initial context
		Queue.Add({SelectedNode, 0, OwnerBP});
		BestDepth.Add(SelectedNode, 0);
		VisitedEntryNodes.Add(SelectedNode);

		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FTraversalItem Current = Queue[i];
			++ForwardQueueProcessed;
			if (!Current.Node || Current.Depth >= ForwardDepth)
			{
				if (Current.Depth >= ForwardDepth)
				{
					++ForwardDepthLimitedSkips;
				}
				continue;
			}

			for (UEdGraphPin* OutPin : Current.Node->Pins)
			{
				if (!IsExecPin(OutPin, EGPD_Output)) continue;
				const FString RouteLabel = NormalizeRouteLabel(OutPin->GetDisplayName().ToString());

				for (UEdGraphPin* LinkedPin : OutPin->LinkedTo)
				{
					UEdGraphNode* Next = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!Next) continue;

					// Same graph - normal traversal
					if (Next->GetGraph() == Current.Node->GetGraph())
					{
						const int32 EdgeCountBefore = Edges.Num();
						AddEdgeUnique(Edges, EdgeKeys, Current.Node, Next, RouteLabel);
						if (Edges.Num() > EdgeCountBefore)
						{
							++ForwardSameGraphEdgesAdded;
						}
						AddOrUpdateNodeMeta(NodeMeta, Next, Current.Depth + 1);

						const int32 NextDepth = Current.Depth + 1;
						if (!BestDepth.Contains(Next) || NextDepth < BestDepth.FindRef(Next))
						{
							BestDepth.Add(Next, NextDepth);
							Queue.Add({Next, NextDepth, Current.ContextBlueprint});
							NodeCallerBlueprint.FindOrAdd(Next) = Current.ContextBlueprint;
							++ForwardSameGraphEnqueued;
							UE_LOG(LogBPExecFlowTracer, VeryVerbose,
							       TEXT("Forward enqueue same-graph: From='%s' To='%s' Depth=%d Route='%s'"),
							       *NodeDisplayName(Current.Node),
							       *NodeDisplayName(Next),
							       NextDepth,
							       *RouteLabel);
						}
					}
				}
				/** CrossBP jump **/
				if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Current.Node))
				{
					++ForwardCrossBPAttempts;

					const UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self);
					const UEdGraphPin* TargetPin = SelfPin ? SelfPin : CallNode->FindPin(TEXT("Target"));

					const UClass* TargetClass = ResolvePinOwnerClass(TargetPin);
					if (!TargetClass)
					{
						if (const UFunction* Func = CallNode->GetTargetFunction())
							TargetClass = Func->GetOwnerClass();
					}

					UBlueprint* TargetBP = GetBlueprintFromClass(TargetClass);

					if (!TargetBP)
					{
						++ForwardCrossBPNoTargetBPSkips;
						UE_LOG(LogBPExecFlowTracer, Verbose,
						       TEXT("Forward cross-BP skip: unable to resolve target Blueprint for call '%s' in node '%s'"),
						       *NodeDisplayName(CallNode),
						       *NodeDisplayName(Current.Node));
					}
					else
					{
						const UFunction* Func = CallNode->GetTargetFunction();
						const FName FunctionName = Func ? Func->GetFName() : NAME_None;
						UEdGraphNode* EntryNode = FindCallableEntryInBlueprint(TargetBP, FunctionName);

						if (!EntryNode)
						{
							++ForwardCrossBPNoEntrySkips;
							UE_LOG(LogBPExecFlowTracer, Verbose,
							       TEXT("Forward cross-BP skip: Blueprint '%s' has no function entry for '%s'"),
							       *GetNameSafe(TargetBP),
							       *FunctionName.ToString());
						}
						else if (VisitedEntryNodes.Contains(EntryNode))
						{
							++ForwardCrossBPVisitedSkips;
							// Still add an edge so this call-site links to the already-visited entry.
							const FString CallRoute = TEXT("Call");
							const int32 EdgeCountBefore = Edges.Num();
							AddEdgeUnique(Edges, EdgeKeys, Current.Node, EntryNode, CallRoute);
							if (Edges.Num() > EdgeCountBefore)
								++ForwardCrossBPSuccesses;
							UE_LOG(LogBPExecFlowTracer, Verbose,
								   TEXT("Forward cross-BP skip: already visited entry node '%s' in Blueprint '%s' via call '%s' (edge added)"),
								   *NodeDisplayName(EntryNode),
								   *GetNameSafe(TargetBP),
								   *NodeDisplayName(CallNode));
						}
						else
						{
							VisitedEntryNodes.Add(EntryNode);  // ← marks EntryNode, not the caller

							const FString CallRoute = TEXT("Call");
							const int32 EdgeCountBefore = Edges.Num();
							AddEdgeUnique(Edges, EdgeKeys, Current.Node, EntryNode, CallRoute);
							if (Edges.Num() > EdgeCountBefore)
								++ForwardCrossBPSuccesses;

							const int32 NextDepth = Current.Depth + 1;
							AddOrUpdateNodeMeta(NodeMeta, EntryNode, NextDepth);

							if (!BestDepth.Contains(EntryNode) || NextDepth < BestDepth.FindRef(EntryNode))
							{
								BestDepth.Add(EntryNode, NextDepth);
								Queue.Add({EntryNode, NextDepth, TargetBP});
								NodeCallerBlueprint.FindOrAdd(EntryNode) = Current.ContextBlueprint;
							}
						}
					}
				}
			}
		}

		UE_LOG(LogBPExecFlowTracer, Log,
		       TEXT(
			       "Forward traversal: Processed=%d DepthLimited=%d SameGraphEdgesAdded=%d SameGraphEnqueued=%d CrossBPAttempts=%d CrossBPSuccesses=%d CrossBPVisitedSkips=%d CrossBPNoTargetBPSkips=%d CrossBPNoEntrySkips=%d VisitedBlueprints=%d"
		       ),
		       ForwardQueueProcessed,
		       ForwardDepthLimitedSkips,
		       ForwardSameGraphEdgesAdded,
		       ForwardSameGraphEnqueued,
		       ForwardCrossBPAttempts,
		       ForwardCrossBPSuccesses,
		       ForwardCrossBPVisitedSkips,
		       ForwardCrossBPNoTargetBPSkips,
		       ForwardCrossBPNoEntrySkips,
		       VisitedEntryNodes.Num());
	}

	// Upstream traversal: prev -> current
	if (BackwardDepth > 0)
	{
		TArray<FTraversalItem> Queue;
		TMap<UEdGraphNode*, int32> BestDepth;

		// FIX 1: pass OwnerBP as initial context
		Queue.Add({SelectedNode, 0, OwnerBP});
		BestDepth.Add(SelectedNode, 0);

		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FTraversalItem Current = Queue[i];
			++BackwardQueueProcessed;
			if (!Current.Node || FMath::Abs(Current.Depth) >= BackwardDepth)
			{
				if (Current.Node && FMath::Abs(Current.Depth) >= BackwardDepth)
				{
					++BackwardDepthLimitedSkips;
				}
				continue;
			}

			for (UEdGraphPin* InPin : Current.Node->Pins)
			{
				if (!IsExecPin(InPin, EGPD_Input)) continue;

				for (UEdGraphPin* LinkedPin : InPin->LinkedTo)
				{
					UEdGraphNode* Prev = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!Prev) continue;

					// FIX 3: explicit skip instead of RootGraph hard block
					if (Prev->GetGraph() != Current.Node->GetGraph())
					{
						++BackwardCrossGraphSkips;
						continue;
					}

					const FString RouteLabel = NormalizeRouteLabel(LinkedPin->GetDisplayName().ToString());
					const int32 EdgeCountBefore = Edges.Num();
					AddEdgeUnique(Edges, EdgeKeys, Prev, Current.Node, RouteLabel);
					if (Edges.Num() > EdgeCountBefore)
					{
						++BackwardEdgesAdded;
					}
					AddOrUpdateNodeMeta(NodeMeta, Prev, Current.Depth - 1);

					const int32 PrevDepth = Current.Depth - 1;
					const int32 ExistingDepth = BestDepth.FindRef(Prev);
					if (!BestDepth.Contains(Prev) || FMath::Abs(PrevDepth) < FMath::Abs(ExistingDepth))
					{
						BestDepth.Add(Prev, PrevDepth);
						Queue.Add({Prev, PrevDepth, Current.ContextBlueprint});
						++BackwardEnqueued;
						UE_LOG(LogBPExecFlowTracer, VeryVerbose,
						       TEXT("Backward enqueue: From='%s' To='%s' Depth=%d Route='%s'"),
						       *NodeDisplayName(Prev),
						       *NodeDisplayName(Current.Node),
						       PrevDepth,
						       *RouteLabel);
					}
				}
			}
		}

		UE_LOG(LogBPExecFlowTracer, Log,
		       TEXT("Backward traversal: Processed=%d DepthLimited=%d CrossGraphSkips=%d EdgesAdded=%d Enqueued=%d"),
		       BackwardQueueProcessed,
		       BackwardDepthLimitedSkips,
		       BackwardCrossGraphSkips,
		       BackwardEdgesAdded,
		       BackwardEnqueued);
	}

	if (NodeMeta.Num() == 0)
	{
		UE_LOG(LogBPExecFlowTracer, Warning, TEXT("TraceFromNode produced no nodes."));
		return Result;
	}

	TArray<TPair<UEdGraphNode*, FNodeMeta>> Nodes;
	for (const TPair<UEdGraphNode*, FNodeMeta>& Pair : NodeMeta)
	{
		Nodes.Add(Pair);
	}

	Nodes.Sort([](const TPair<UEdGraphNode*, FNodeMeta>& A, const TPair<UEdGraphNode*, FNodeMeta>& B)
	{
		if (A.Value.Depth == B.Value.Depth)
		{
			return NodeDisplayName(A.Key) < NodeDisplayName(B.Key);
		}
		return A.Value.Depth < B.Value.Depth;
	});

	TMap<UEdGraphNode*, int32> NodeToGroup;
	for (const TPair<UEdGraphNode*, FNodeMeta>& Pair : Nodes)
	{
		UEdGraphNode* Node = Pair.Key;
		const FNodeMeta& Meta = Pair.Value;

		FExecBPGroup Group;
		UBlueprint* NodeOwnerBlueprint = nullptr;
		Group.BlueprintName = ResolveNodeOwnerLabel(Node, OwnerBP, NodeOwnerBlueprint);
		Group.ClusterBlueprintName = Group.BlueprintName; // default to owner
		Group.DepthColumn = Meta.Depth;
		Group.SourceBlueprint = NodeOwnerBlueprint ? NodeOwnerBlueprint : OwnerBP;

		FExecFuncEntry Entry;
		Entry.FunctionName = FName(*NodeDisplayName(Node));
		Entry.DisplayName = NodeDisplayName(Node);
		Entry.Kind = GetKind(Node);
		Entry.SourceNode = Node;
		Entry.bIsRoot = (Node == SelectedNode);

		// Override cluster to caller's BP for cross-BP call-site nodes
		if (UBlueprint* const* CallerBPPtr = NodeCallerBlueprint.Find(Node))
		{
			UBlueprint* CallerBP = *CallerBPPtr;
			if (CallerBP && NodeOwnerBlueprint && CallerBP != NodeOwnerBlueprint)
			{
				if (Entry.Kind == EExecNodeKind::ExecStep || Entry.Kind == EExecNodeKind::Function)
				{
					Group.ClusterBlueprintName = CallerBP->GetName();
				}
			}
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsExecPin(Pin, EGPD_Output)) continue;
			const FString Route = NormalizeRouteLabel(Pin->GetDisplayName().ToString());
			if (!Route.IsEmpty() && Route != TEXT("Exec"))
				Entry.OutgoingRouteLabels.AddUnique(Route);
		}

		Entry.OutgoingRouteLabels.Sort([](const FString& A, const FString& B)
		{
			auto IsPrimary = [](const FString& R) -> bool
			{
				const FString L = R.ToLower();
				return L.Contains(TEXT("true")) ||
					(L.Contains(TEXT("valid")) && !L.Contains(TEXT("not")));
			};
			if (IsPrimary(A) != IsPrimary(B)) return IsPrimary(A);
			return A < B;
		});

		Group.Functions.Add(MoveTemp(Entry));

		const int32 GroupIdx = Result.Groups.Add(MoveTemp(Group));
		NodeToGroup.Add(Node, GroupIdx);
	}

	Result.RootGroupIndex = NodeToGroup.FindRef(SelectedNode);

	for (const FEdgeRecord& Edge : Edges)
	{
		const int32* FromIdx = NodeToGroup.Find(Edge.From);
		const int32* ToIdx = NodeToGroup.Find(Edge.To);
		if (!FromIdx || !ToIdx || *FromIdx == *ToIdx)
		{
			continue;
		}
		Result.Edges.AddUnique(FExecFlowEdge{*FromIdx, *ToIdx, Edge.Route});
	}

	UE_LOG(LogBPExecFlowTracer, Log,
	       TEXT("TraceFromNode end: Groups=%d Edges=%d NodeMeta=%d RootGroupIndex=%d Selected='%s'"),
	       Result.Groups.Num(),
	       Result.Edges.Num(),
	       NodeMeta.Num(),
	       Result.RootGroupIndex,
	       *NodeDisplayName(SelectedNode));

	return Result;
}
