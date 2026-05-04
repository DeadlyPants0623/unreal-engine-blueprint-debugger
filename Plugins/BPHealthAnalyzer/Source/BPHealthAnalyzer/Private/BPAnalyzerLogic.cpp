#include "BPAnalyzerLogic.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CreateDelegate.h"  // detect functions used via Bind Event / Create Event
#include "Kismet2/BlueprintEditorUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/DataAsset.h"       // detect Primary/Data Asset blueprints

// Functions called implicitly by the engine — never from within the Blueprint itself.
// Exclude these from "unused function" warnings.
static const TArray<FName> EngineImplicitFunctions = {
	FName(TEXT("UserConstructionScript")),
	FName(TEXT("ExecuteUbergraph")), // base; generated variants like ExecuteUbergraph_BP_Foo caught via StartsWith
};


TArray<FBPIssue> FBPAnalyzerLogic::AnalyzeAllBlueprints()
{
	TArray<FBPIssue> AllIssues;

	FAssetRegistryModule& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.Get().GetAssetsByClass(
		UBlueprint::StaticClass()->GetClassPathName(),
		BlueprintAssets
	);

	// ---- Pass 1: collect all project Blueprints ----
	TArray<UBlueprint*> AllBPs;
	for (const FAssetData& AssetData : BlueprintAssets)
	{
		const FString PackagePathStr = AssetData.PackagePath.ToString();

		// Skip engine core content (/Engine/...)
		if (PackagePathStr.StartsWith(TEXT("/Engine"))) continue;

		// Fix 3: also skip engine PLUGIN content — engine plugins mount under their own
		// root (e.g. /Niagara/, /DecalContent/) not under /Engine/, so we must check
		// IPluginManager to see if the mount point is an engine-shipped plugin.
		{
			// Extract first path segment as mount point  e.g. "/Niagara/Foo" → "Niagara"
			FString MountPoint = PackagePathStr.RightChop(1);
			int32 SlashIdx;
			if (MountPoint.FindChar(TEXT('/'), SlashIdx))
				MountPoint = MountPoint.Left(SlashIdx);

			TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(MountPoint);
			if (Plugin.IsValid() && Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine)
				continue;
		}

		UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());
		if (!BP) continue;

		// Skip Blueprint Interfaces — pure declarations, analyzing them produces only noise
		if (BP->BlueprintType == BPTYPE_Interface) continue;

		// Skip Macro Libraries — macros expand inline, cross-BP checks can't trace them
		if (BP->BlueprintType == BPTYPE_MacroLibrary) continue;

		AllBPs.Add(BP);
	}

	// ---- Pass 2: build cross-BP call set (Fix 4) ----
	// Key: "GeneratedClassName::FunctionName" for every CallFunction node found in any BP.
	// Manager BPs whose functions are called by other BPs won't be flagged as unused.
	TSet<FString> CrossBPCallSet;
	for (UBlueprint* BP : AllBPs)
	{
		TArray<UEdGraph*> AllGraphs;
		BP->GetAllGraphs(AllGraphs);
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				// CallFunction nodes
				if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
				{
					UClass* ParentClass = CallNode->FunctionReference.GetMemberParentClass();
					if (ParentClass)
					{
						CrossBPCallSet.Add(
							ParentClass->GetName() + TEXT("::") + CallNode->FunctionReference.GetMemberName().ToString()
						);
					}
				}
				// Fix 2: CreateDelegate nodes (Bind Event / Create Event) reference a function
				// by name but never emit a UK2Node_CallFunction — add them to the call set too.
				if (UK2Node_CreateDelegate* CreateNode = Cast<UK2Node_CreateDelegate>(Node))
				{
					FName DelegFuncName = CreateNode->GetFunctionName();
					UClass* ScopeClass  = CreateNode->GetScopeClass();
					if (ScopeClass && DelegFuncName != NAME_None)
					{
						CrossBPCallSet.Add(ScopeClass->GetName() + TEXT("::") + DelegFuncName.ToString());
					}
				}
			}
		}
	}

	// ---- Pass 3: analyze each BP with the cross-reference set ----
	for (UBlueprint* BP : AllBPs)
	{
		TArray<FBPIssue> Issues = AnalyzeBlueprint(BP, CrossBPCallSet);
		AllIssues.Append(Issues);
	}

	return AllIssues;
}

TArray<FBPIssue> FBPAnalyzerLogic::AnalyzeBlueprint(UBlueprint* Blueprint, const TSet<FString>& CrossBPCallSet)
{
	TArray<FBPIssue> Issues;
	if (!Blueprint) return Issues;

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;
		CheckDisconnectedExecPins(Blueprint, Graph, Issues);
		CheckNodesWithNoOutputConnections(Blueprint, Graph, Issues);
	}

	CheckUnusedVariables(Blueprint, Issues);
	CheckUnusedFunctions(Blueprint, CrossBPCallSet, Issues);

	return Issues;
}

void FBPAnalyzerLogic::CheckDisconnectedExecPins(UBlueprint* BP, UEdGraph* Graph, TArray<FBPIssue>& OutIssues)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		// Pre-count exec output pins on this node (Fix 3: Branch/Sequence/Switch support)
		int32 TotalExecOutputs    = 0;
		int32 ConnectedExecOutputs = 0;
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				TotalExecOutputs++;
				if (P->LinkedTo.Num() > 0) ConnectedExecOutputs++;
			}
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() != 0) continue;

			// Skip return nodes — intentionally left open
			if (Node->IsA<UK2Node_FunctionResult>()) continue;

			// Skip completely isolated stubs: event nodes (BeginPlay, Tick…) or function
			// entry nodes (Construction Script…) where NOTHING at all is connected
			if (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>())
			{
				bool bHasAnyConnection = false;
				for (UEdGraphPin* AnyPin : Node->Pins)
				{
					if (AnyPin->LinkedTo.Num() > 0) { bHasAnyConnection = true; break; }
				}
				if (!bHasAnyConnection) continue;
			}

			// Multi-output nodes (Branch True/False, Sequence, Switch): if at least one exec
			// output is connected, the unconnected ones are almost always intentional
			// (e.g. Branch False left open, Sequence last pin unused). Silently skip.
			if (TotalExecOutputs > 1 && ConnectedExecOutputs > 0) continue;

			// Single-output-exec node whose input exec IS connected → it is the terminal
			// node in this execution chain (e.g. SpawnActor, DestroyActor, Print String).
			// Leaving its output exec open is intentional and valid — do not flag.
			if (TotalExecOutputs == 1)
			{
				bool bHasConnectedInputExec = false;
				for (UEdGraphPin* AnyPin : Node->Pins)
				{
					if (AnyPin->Direction == EGPD_Input &&
						AnyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
						AnyPin->LinkedTo.Num() > 0)
					{
						bHasConnectedInputExec = true;
						break;
					}
				}
				if (bHasConnectedInputExec) continue;
			}

			FBPIssue Issue;
			Issue.BlueprintName = BP->GetName();
			Issue.GraphName     = Graph->GetName();
			Issue.NodeName      = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Issue.IssueDescription = FString::Printf(
				TEXT("Output exec pin '%s' is not connected — execution stops here"),
				*Pin->GetName()
			);
			Issue.Severity        = 1; // Error
			Issue.NodeGuid        = Node->NodeGuid;
			Issue.SourceBlueprint = BP;
			Issue.SourceNode      = Node;
			OutIssues.Add(Issue);
		}
	}
}

void FBPAnalyzerLogic::CheckNodesWithNoOutputConnections(UBlueprint* BP, UEdGraph* Graph, TArray<FBPIssue>& OutIssues)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		// Skip pure nodes (getters etc)
		UK2Node* K2Node = Cast<UK2Node>(Node);
		if (K2Node && K2Node->IsNodePure()) continue;

		// Skip event-like entry nodes: anything with output exec pins but NO input exec pin
		// is a graph entry point (BeginPlay, InputKey, Debug Key Z, Custom Event, etc.).
		// Their data output pins (e.g. the Key pin on InputKey) are informational — never "discarded".
		// This pin-structure check is class-hierarchy agnostic and catches all event node types.
		{
			bool bHasInputExec  = false;
			bool bHasOutputExec = false;
			for (UEdGraphPin* P : Node->Pins)
			{
				if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					if (P->Direction == EGPD_Input)  bHasInputExec  = true;
					if (P->Direction == EGPD_Output) bHasOutputExec = true;
				}
			}
			if (bHasOutputExec && !bHasInputExec) continue; // event / entry node
		}

		// Skip variable Set nodes — their output pin is a convenience passthrough,
		// not using it is completely normal and expected
		if (Node->IsA<UK2Node_VariableSet>()) continue;

		// Skip array mutation nodes (Add, Remove, Insert, etc.) — return values (index,
		// success bool) are almost never needed.
		if (Node->IsA<UK2Node_CallArrayFunction>()) continue;


		// Fix 1 (general): Skip nodes that are fully threaded into an execution chain
		// (both input exec and output exec are connected). When a developer intentionally
		// wires a function call into a chain (e.g. AddInstance, SpawnDecal, etc.) but
		// doesn't use the return value, that is a deliberate choice — not a bug.
		{
			bool bHasConnectedInputExec  = false;
			bool bHasConnectedOutputExec = false;
			for (UEdGraphPin* P : Node->Pins)
			{
				if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					if (P->Direction == EGPD_Input  && P->LinkedTo.Num() > 0) bHasConnectedInputExec  = true;
					if (P->Direction == EGPD_Output && P->LinkedTo.Num() > 0) bHasConnectedOutputExec = true;
				}
			}
			if (bHasConnectedInputExec && bHasConnectedOutputExec) continue;
		}

		bool bHasAnyOutput    = false;
		bool bHasAnyOutputPin = false;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output &&
				Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				bHasAnyOutputPin = true;
				if (Pin->LinkedTo.Num() > 0) { bHasAnyOutput = true; break; }
			}
		}

		if (bHasAnyOutputPin && !bHasAnyOutput)
		{
			FBPIssue Issue;
			Issue.BlueprintName    = BP->GetName();
			Issue.GraphName        = Graph->GetName();
			Issue.NodeName         = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Issue.IssueDescription = TEXT("Node has output data pins but none are connected — result is being discarded");
			Issue.Severity         = 0; // Warning
			Issue.NodeGuid         = Node->NodeGuid;
			Issue.SourceBlueprint  = BP;
			Issue.SourceNode       = Node;
			OutIssues.Add(Issue);
		}
	}
}

void FBPAnalyzerLogic::CheckUnusedVariables(UBlueprint* BP, TArray<FBPIssue>& OutIssues)
{
	// Skip Data Asset blueprints entirely — their variables are config/data fields
	// populated in the asset editor, not accessed via graph Get/Set nodes.
	// Checking them would produce only false positives.
	if (BP->ParentClass && BP->ParentClass->IsChildOf(UDataAsset::StaticClass()))
		return;

	TArray<UEdGraph*> AllGraphs;
	BP->GetAllGraphs(AllGraphs);

	for (FBPVariableDescription& Var : BP->NewVariables)
	{
		bool bIsUsed = false;
		FName VarName = Var.VarName;

		// Fix 1: Skip Event Dispatchers — they are multicast delegates stored in NewVariables
		// but used via Bind/Unbind/Call/Assign nodes, not Get/Set. They are never "unused"
		// in the traditional sense; they are the mechanism for broadcasting events.
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate) continue;
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Delegate)   continue;

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
				{
					if (GetNode->GetVarName() == VarName) { bIsUsed = true; break; }
				}
				if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
				{
					if (SetNode->GetVarName() == VarName) { bIsUsed = true; break; }
				}
			}
			if (bIsUsed) break;
		}

		if (!bIsUsed)
		{
			FBPIssue Issue;
			Issue.BlueprintName    = BP->GetName();
			Issue.GraphName        = TEXT("Variables");
			Issue.NodeName         = VarName.ToString();
			Issue.IssueDescription = FString::Printf(
				TEXT("Variable '%s' is declared but never read or written"), *VarName.ToString());
			Issue.Severity        = 0; // Warning
			Issue.SourceBlueprint = BP;
			OutIssues.Add(Issue);
		}
	}
}

void FBPAnalyzerLogic::CheckUnusedFunctions(UBlueprint* BP, const TSet<FString>& CrossBPCallSet, TArray<FBPIssue>& OutIssues)
{
	TArray<UEdGraph*> AllGraphs;
	BP->GetAllGraphs(AllGraphs);

	for (UEdGraph* FuncGraph : BP->FunctionGraphs)
	{
		if (!FuncGraph) continue;
		FName FuncName = FuncGraph->GetFName();

		// Skip functions called implicitly by the engine (UserConstructionScript, ExecuteUbergraph_*)
		bool bIsEngineImplicit = false;
		for (const FName& ImplicitName : EngineImplicitFunctions)
		{
			if (FuncName == ImplicitName ||
				FuncName.ToString().StartsWith(ImplicitName.ToString()))
			{
				bIsEngineImplicit = true;
				break;
			}
		}
		if (bIsEngineImplicit) continue;

		bool bIsCalled = false;

		// First: check within this Blueprint
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				// Direct calls
				UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
				if (CallNode && CallNode->FunctionReference.GetMemberName() == FuncName)
				{
					bIsCalled = true;
					break;
				}
				// Fix 2: "Create Event" / "Bind Event" references the function via
				// UK2Node_CreateDelegate, not UK2Node_CallFunction.
				if (UK2Node_CreateDelegate* CreateNode = Cast<UK2Node_CreateDelegate>(Node))
				{
					if (CreateNode->GetFunctionName() == FuncName)
					{
						bIsCalled = true;
						break;
					}
				}
			}
			if (bIsCalled) break;
		}

		// Fix 4: cross-Blueprint check — manager BPs expose functions called by other BPs
		if (!bIsCalled && BP->GeneratedClass)
		{
			FString CrossKey = BP->GeneratedClass->GetName() + TEXT("::") + FuncName.ToString();
			if (CrossBPCallSet.Contains(CrossKey))
			{
				bIsCalled = true;
			}
		}

		if (!bIsCalled)
		{
			FBPIssue Issue;
			Issue.BlueprintName    = BP->GetName();
			Issue.GraphName        = TEXT("Functions");
			Issue.NodeName         = FuncName.ToString();
			Issue.IssueDescription = FString::Printf(
				TEXT("Function '%s' is defined but never called within this Blueprint or by any other Blueprint"),
				*FuncName.ToString()
			);
			Issue.Severity        = 0; // Warning
			Issue.SourceBlueprint = BP;

			for (UEdGraphNode* Node : FuncGraph->Nodes)
			{
				if (Node && Node->IsA<UK2Node_FunctionEntry>())
				{
					Issue.SourceNode = Node;
					Issue.NodeGuid   = Node->NodeGuid;
					break;
				}
			}

			OutIssues.Add(Issue);
		}
	}
}
