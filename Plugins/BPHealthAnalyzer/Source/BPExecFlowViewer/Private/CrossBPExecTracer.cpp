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
		if (Lower.Contains(TEXT("is not valid")) || Lower.Contains(TEXT("not valid"))) return
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

				return TargetClass->GetName();
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
					return OwnerClass->GetName();
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
	
	UK2Node_FunctionEntry* FindFunctionEntryInBlueprint(UBlueprint* BP, FName FunctionName)
	{
		if (!BP) return nullptr;
		
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (Graph->GetFName() != FunctionName) continue;
			
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
					return Entry;
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
		return Result;
	}

	UEdGraph* RootGraph = SelectedNode->GetGraph();
	UBlueprint* OwnerBP = RootGraph->GetTypedOuter<UBlueprint>();

	TMap<UEdGraphNode*, FNodeMeta> NodeMeta;
	TArray<FEdgeRecord> Edges;
	TSet<FString> EdgeKeys;

	AddOrUpdateNodeMeta(NodeMeta, SelectedNode, 0);

	// Downstream traversal: current -> next
	if (ForwardDepth > 0)
	{
		TArray<FTraversalItem> Queue;
		TMap<UEdGraphNode*, int32> BestDepth;
		Queue.Add({SelectedNode, 0});
		BestDepth.Add(SelectedNode, 0);

		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FTraversalItem Current = Queue[i];
			if (!Current.Node || Current.Depth >= ForwardDepth) continue;

			for (UEdGraphPin* OutPin : Current.Node->Pins)
			{
				if (!IsExecPin(OutPin, EGPD_Output)) continue;
				// Use display name so Branch "then" pin → "true" → "Branch: True"
				const FString RouteLabel = NormalizeRouteLabel(OutPin->GetDisplayName().ToString());

				for (UEdGraphPin* LinkedPin : OutPin->LinkedTo)
				{
					UEdGraphNode* Next = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!Next || Next->GetGraph() != RootGraph) continue;

					AddEdgeUnique(Edges, EdgeKeys, Current.Node, Next, RouteLabel);
					AddOrUpdateNodeMeta(NodeMeta, Next, Current.Depth + 1);


					const int32 NextDepth = Current.Depth + 1;
					const int32 ExistingDepth = BestDepth.FindRef(Next);
					if (!BestDepth.Contains(Next) || NextDepth < ExistingDepth)
					{
						BestDepth.Add(Next, NextDepth);
						Queue.Add({Next, NextDepth});
					}
				}
			}
		}
	}

	// Upstream traversal: prev -> current
	if (BackwardDepth > 0)
	{
		TArray<FTraversalItem> Queue;
		TMap<UEdGraphNode*, int32> BestDepth;
		Queue.Add({SelectedNode, 0});
		BestDepth.Add(SelectedNode, 0);

		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FTraversalItem Current = Queue[i];
			if (!Current.Node || FMath::Abs(Current.Depth) >= BackwardDepth) continue;

			for (UEdGraphPin* InPin : Current.Node->Pins)
			{
				if (!IsExecPin(InPin, EGPD_Input)) continue;

				for (UEdGraphPin* LinkedPin : InPin->LinkedTo)
				{
					UEdGraphNode* Prev = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (!Prev || Prev->GetGraph() != RootGraph) continue;

					// Use display name for same Branch-node consistency
					const FString RouteLabel = NormalizeRouteLabel(LinkedPin->GetDisplayName().ToString());
					AddEdgeUnique(Edges, EdgeKeys, Prev, Current.Node, RouteLabel);
					AddOrUpdateNodeMeta(NodeMeta, Prev, Current.Depth - 1);


					const int32 PrevDepth = Current.Depth - 1;
					const int32 ExistingDepth = BestDepth.FindRef(Prev);
					if (!BestDepth.Contains(Prev) || FMath::Abs(PrevDepth) < FMath::Abs(ExistingDepth))
					{
						BestDepth.Add(Prev, PrevDepth);
						Queue.Add({Prev, PrevDepth});
					}
				}
			}
		}
	}

	if (NodeMeta.Num() == 0)
	{
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
		Group.DepthColumn = Meta.Depth;
		Group.SourceBlueprint = NodeOwnerBlueprint ? NodeOwnerBlueprint : OwnerBP;

		FExecFuncEntry Entry;
		Entry.FunctionName = FName(*NodeDisplayName(Node));
		Entry.DisplayName = NodeDisplayName(Node);
		Entry.Kind = GetKind(Node);
		Entry.SourceNode = Node;
		Entry.bIsRoot = (Node == SelectedNode);

		// Collect ALL semantic exec-output routes from the node's own pins,
		// regardless of whether their targets are within the traversal scope.
		// This ensures Branch/IsValid always shows both outputs.
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsExecPin(Pin, EGPD_Output)) continue;
			const FString Route = NormalizeRouteLabel(Pin->GetDisplayName().ToString());
			if (!Route.IsEmpty() && Route != TEXT("Exec"))
				Entry.OutgoingRouteLabels.AddUnique(Route);
		}

		// Sort: True / Valid first (upper pin), False / Not Valid second (lower pin)
		Entry.OutgoingRouteLabels.Sort([](const FString& A, const FString& B)
		{
			auto IsPrimary = [](const FString& R) -> bool
			{
				const FString L = R.ToLower();
				return L.Contains(TEXT("true")) ||
					(L.Contains(TEXT("valid")) && !L.Contains(TEXT("not")));
			};
			if (IsPrimary(A) != IsPrimary(B)) return IsPrimary(A); // primary first
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

	return Result;
}
