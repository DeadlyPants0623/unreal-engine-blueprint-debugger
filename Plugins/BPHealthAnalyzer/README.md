# BPExecFlowViewer — Developer Reference

> Plugin package: **BPHealthAnalyzer** (`BPHealthAnalyzer.uplugin`)  
> Engine: **Unreal Engine 5.7** | Module type: **Editor only** | Version: 1.0  
> Author: CJ

---

## Table of Contents

1. [Plugin Overview](#1-plugin-overview)
2. [Repository Structure](#2-repository-structure)
3. [Module: BPExecFlowViewer — Architecture Overview](#3-module-bpexecflowviewer--architecture-overview)
4. [Data Types (`ExecFlowTypes.h`)](#4-data-types-execflowtypesh)
5. [Tracer (`CrossBPExecTracer`)](#5-tracer-crossbpexectracer)
6. [Graph Layer (`ExecFlowGraph`)](#6-graph-layer-execflowgraph)
7. [Slate Widget: `SExecFlowGraphNode`](#7-slate-widget-sexecflowgraphnode)
8. [Slate Widget: `SExecLocalPathWidget`](#8-slate-widget-sexeclocalpathwidget)
9. [Module Entry Point (`FBPExecFlowViewerModule`)](#9-module-entry-point-fbpexecflowviewermodule)
10. [Full Data-Flow Pipeline](#10-full-data-flow-pipeline)
11. [Module: BPHealthAnalyzer](#11-module-bphealthanalyzer)
12. [Build Configuration](#12-build-configuration)
13. [Key UE5 APIs Used](#13-key-ue5-apis-used)
14. [Known Limitations & What Is Not Yet Implemented](#14-known-limitations--what-is-not-yet-implemented)
15. [Planned Next Steps / Where to Continue](#15-planned-next-steps--where-to-continue)
16. [Coding Conventions Enforced in This Plugin](#16-coding-conventions-enforced-in-this-plugin)

---

## 1. Plugin Overview

The `.uplugin` file (`BPHealthAnalyzer.uplugin`) packages **two independent editor modules**:

| Module | Purpose |
|--------|---------|
| `BPHealthAnalyzer` | Static analysis — scans all Blueprints for disconnected exec pins, unused variables, unused functions, and nodes with no output connections. Opened from the Level Editor toolbar / Window menu. |
| `BPExecFlowViewer` | **Runtime-graph-independent** execution flow visualiser. Right-click any Blueprint node → "View Exec Flow" → opens a nomad docking tab showing a live directed graph of execution neighbours. |

Both modules load at phase `Default` and are Editor-only (`"Type": "Editor"`).

---

## 2. Repository Structure

```
Plugins/BPHealthAnalyzer/
├── BPHealthAnalyzer.uplugin          ← plugin manifest (two modules declared)
├── Resources/
│   ├── Icon128.png
│   └── PlaceholderButtonIcon.svg
└── Source/
    ├── BPExecFlowViewer/             ← execution-flow visualiser module
    │   ├── BPExecFlowViewer.Build.cs
    │   ├── Public/
    │   │   └── BPExecFlowViewer.h    ← FBPExecFlowViewerModule declaration
    │   └── Private/
    │       ├── BPExecFlowViewer.cpp  ← module startup/shutdown, context-menu wiring
    │       ├── ExecFlowTypes.h       ← pure data structs / enums (no UE reflection)
    │       ├── ExecFlowGraph.h/.cpp  ← UObject graph layer (schema, node, graph)
    │       ├── CrossBPExecTracer.h/.cpp ← graph traversal engine
    │       ├── SExecFlowGraphNode.h/.cpp ← custom SGraphNode widget + factory
    │       └── SExecLocalPathWidget.h/.cpp ← root panel widget (tab content)
    └── BPHealthAnalyzer/             ← static analysis module
        ├── BPHealthAnalyzer.Build.cs
        ├── Public/
        │   ├── BPHealthAnalyzer.h
        │   ├── BPHealthAnalyzerCommands.h
        │   └── BPHealthAnalyzerStyle.h
        └── Private/
            ├── BPHealthAnalyzer.cpp
            ├── BPHealthAnalyzerCommands.cpp
            ├── BPHealthAnalyzerStyle.cpp
            ├── BPAnalyzerLogic.h/.cpp ← all static analysis logic
            ├── SBPHealthAnalyzerWidget.h/.cpp ← results panel UI
            └── (ExecFlowGraph, ExecMapBuilder, SExecFlow* — older copies, superseded)
```

> **Important:** The `BPHealthAnalyzer/Private/` folder contains some older, now-duplicate copies of exec-flow files (`ExecFlowGraph`, `ExecMapBuilder`, `SExecFlowGraphNode`, `SExecFlowViewerWidget`, `ExecFlowViewerIntegration`). Those are **not the canonical source** — the canonical files live exclusively in `BPExecFlowViewer/Private/`. The BPHealthAnalyzer module's `.Build.cs` does **not** declare a dependency on BPExecFlowViewer, so the two modules are fully independent at the linker level.

---

## 3. Module: BPExecFlowViewer — Architecture Overview

The module is split into four distinct layers, each owning a clear responsibility:

```
User right-clicks node
        │
        ▼
FBPExecFlowViewerModule          ← Layer 1: Module / wiring
  registers context menu entry
  opens nomad tab
        │
        ▼
SExecLocalPathWidget             ← Layer 2: UI container (root panel)
  owns depth spinners + Rebuild button
  owns SGraphEditor
        │
        ▼
FCrossBPExecTracer::TraceFromNode ← Layer 3: Graph traversal engine
  produces FExecFlowMap (pure data)
        │
        ▼
UExecFlowGraph::PopulateGraph    ← Layer 4: UObject graph + layout
  creates UExecFlowGraphNode objects
  assigns positions (lane-based layout)
  wires pins with MakeLinkTo
        │
        ▼
SExecFlowGraphNode               ← Layer 4b: Custom SGraphNode widget
  renders inside SGraphEditor
  click-to-navigate rows
```

---

## 4. Data Types (`ExecFlowTypes.h`)

This header is **pure C++ with no UE reflection macros**. It defines everything passed between the tracer and the graph layer.

### `EExecNodeKind` (enum class, uint8)

Classifies each discovered node. Used only for display (icon badges and text coloring):

| Value | Meaning |
|-------|---------|
| `Function` | `UK2Node_CallFunction` |
| `Event` | `UK2Node_Event` |
| `CustomEvent` | `UK2Node_CustomEvent` |
| `EventDispatcher` | Event Dispatch calls |
| `Macro` | `UK2Node_MacroInstance` |
| `ExecStep` | Everything else with exec pins |

---

### `FExecFuncEntry` (struct)

One **row** inside a rendered node card. Holds:

| Field | Type | Description |
|-------|------|-------------|
| `FunctionName` | `FName` | Internal function/node name |
| `DisplayName` | `FString` | Human-readable title (from `GetNodeTitle`) |
| `Kind` | `EExecNodeKind` | Used for icon badge |
| `bIsRoot` | `bool` | True = the node the user right-clicked |
| `bIsCycleTruncated` | `bool` | True = traversal was cut because revisiting a seen node |
| `SourceNode` | `TWeakObjectPtr<UEdGraphNode>` | Back-pointer to the original Blueprint node; used for click-to-navigate |
| `IntraGraphExecPath` | `FString` | Reserved / unused in current implementation |
| `OutgoingRouteLabels` | `TArray<FString>` | Semantic exec-output labels collected from ALL output exec pins of this node (e.g. `"Branch: True"`, `"Branch: False"`, `"IsValid: Valid"`, `"IsValid: Not Valid"`). Drives which output pins are created on `UExecFlowGraphNode`. |

---

### `FExecBPGroup` (struct)

One **container node** in the flow graph. Under the current "one entry per render node" expansion each group holds exactly one `FExecFuncEntry`.

| Field | Type | Description |
|-------|------|-------------|
| `BlueprintName` | `FString` | Title rendered in the node header bar |
| `DepthColumn` | `int32` | `< 0` = upstream callers, `0` = root, `> 0` = downstream callees |
| `SourceBlueprint` | `TWeakObjectPtr<UBlueprint>` | Owning Blueprint; used for click fallback navigation |
| `Functions` | `TArray<FExecFuncEntry>` | Entries rendered as rows in the node body |
| `bIsSynthetic` | `bool` | Set to true when the group was produced by the expansion step in `PopulateGraph` (original group had multiple entries; each entry gets its own node) |

---

### `FExecFlowEdge` (struct)

One **directed edge** in the flow map.

| Field | Type | Description |
|-------|------|-------------|
| `FromIdx` | `int32` | Index into `FExecFlowMap::Groups` |
| `ToIdx` | `int32` | Index into `FExecFlowMap::Groups` |
| `RouteLabel` | `FString` | Normalised semantic label (e.g. `"Branch: True"`, `"Exec"`, `""`) |

Edges implement `operator==` so they can be deduplicated with `TArray::AddUnique`.

---

### `FExecFlowMap` (struct)

The final product handed from the tracer to the graph layer:

| Field | Type | Description |
|-------|------|-------------|
| `Groups` | `TArray<FExecBPGroup>` | All nodes |
| `Edges` | `TArray<FExecFlowEdge>` | All directed edges |
| `RootGroupIndex` | `int32` | Index of the selected node's group |

---

## 5. Tracer (`CrossBPExecTracer`)

**File:** `CrossBPExecTracer.h` / `CrossBPExecTracer.cpp`

A **stateless utility class** (no instances, no UObject). All work is done in one static method:

```cpp
static FExecFlowMap FCrossBPExecTracer::TraceFromNode(
    UEdGraphNode* SelectedNode,
    int32 BackwardDepth = 1,   // max upstream hops
    int32 ForwardDepth  = 3    // max downstream hops
);
```

### What it does

1. **Validates** the selected node and its owning `UEdGraph`.
2. Runs a **downstream BFS** (depth-first iteration via index loop):
   - Iterates `EGPD_Output` exec pins of each visited node.
   - Follows `LinkedTo` to next nodes **in the same graph only**.
   - Records `FEdgeRecord{From, To, Route}` per link.
   - Tracks best (shallowest) depth per node with a `TMap<UEdGraphNode*, int32> BestDepth`.
3. Runs an **upstream BFS**:
   - Same pattern but follows `EGPD_Input` exec pins backwards.
   - Depth is negative for upstream nodes.
4. **Route label normalisation** (`NormalizeRouteLabel`):
   - Uses `Pin->GetDisplayName()` (not `Pin->PinName`) so that UE-renamed pins like `then`/`else` get human-readable names.
   - Maps: `"true"` → `"Branch: True"`, `"false"/"else"` → `"Branch: False"`, `"is valid"` → `"IsValid: Valid"`, `"is not valid"` → `"IsValid: Not Valid"`, `"then"/"exec"` → `"Exec"`.
5. **Node display name** resolution: uses `GetNodeTitle(ENodeTitleType::ListView)`, falling back to class name.
6. **Owner Blueprint resolution** (`ResolveNodeOwnerLabel`): for `UK2Node_CallFunction` nodes attempts to trace through the `Self`/`Target` pin, then through `GetTargetFunction()->GetOwnerClass()` to find any Blueprint the function belongs to. For everything else uses the graph's `GetTypedOuter<UBlueprint>()`.
7. **Assembles** `FExecFlowMap` from collected metadata, deduplicates edges with `AddUnique`.

### Scope constraint

The tracer deliberately constrains traversal to `Node->GetGraph() == RootGraph`. It does **not** follow `UK2Node_CallFunction` targets into other graphs/blueprints at the trace level. The name `CrossBPExecTracer` is aspirational — cross-Blueprint traversal is **not yet implemented** (see §14).

---

## 6. Graph Layer (`ExecFlowGraph`)

**Files:** `ExecFlowGraph.h` / `ExecFlowGraph.cpp`

This layer converts the pure-data `FExecFlowMap` into a UObject `UEdGraph` that `SGraphEditor` can render.

### `UExecFlowGraphSchema` (UCLASS, extends UEdGraphSchema)

Minimal read-only schema. It exists solely so `SGraphEditor` does not crash needing a valid schema pointer.

| Override | Behaviour |
|----------|-----------|
| `GetPinTypeColor` | Always returns `FLinearColor::White` |
| `CanCreateConnection` | Always returns `CONNECT_RESPONSE_DISALLOW` |
| `ShouldHidePinDefaultValue` | Always returns `true` |
| `BreakPinLinks`, `BreakSinglePinLink` | No-ops — graph is read-only |

---

### `UExecFlowGraphNode` (UCLASS, extends UEdGraphNode)

One visual node per `FExecBPGroup`. Holds `FExecBPGroup GroupData` as a plain member (not a UPROPERTY; it is transient graph data).

**Pin layout (created in `AllocateDefaultPins`):**

- One `EGPD_Input` exec pin named `"In"` (left side).
- One or more `EGPD_Output` exec pins (right side). The names come from `OutgoingRouteLabels` across all function entries. If nothing is collected, a single `"Out"` pin is created. Output pins are sorted: `True`/`Valid` first, then `False`/`Not Valid`, then alphabetical.

**Visual properties:**

| Method | Logic |
|--------|-------|
| `GetNodeTitle` | Returns `GroupData.BlueprintName` |
| `GetTooltipText` | Lists all function entries with kind/path/route annotations |
| `GetNodeTitleColor` | Blue `(0.30, 0.55, 1.00)` for callers (depth < 0), Gold `(1.00, 0.80, 0.20)` for root (depth == 0), Green `(0.25, 0.90, 0.45)` for callees (depth > 0) |

**Pin accessors:**

- `GetInputPin()` — first `EGPD_Input` pin.
- `GetOutputPin()` — first `EGPD_Output` pin (fallback).
- `GetOutputPinForRoute(const FString& CompactRoute)` — finds the output pin whose `PinName` matches the compact pin name (e.g. `"True"`, `"Valid"`, `"Not Valid"`, `"Out"`).

---

### `UExecFlowGraph` (UCLASS, extends UEdGraph)

Container for all `UExecFlowGraphNode` objects. Two static helpers:

#### `UExecFlowGraph* Create(UObject* Outer)`

Creates a new graph, sets `Schema = UExecFlowGraphSchema::StaticClass()`.

#### `bool PopulateGraph(UExecFlowGraph* Graph, const FExecFlowMap& FlowMap)`

The full build-and-layout pipeline. Steps in order:

1. **Group expansion** — splits each `FExecBPGroup` with N functions into N separate `FExecBPGroup`s marked `bIsSynthetic = true`, each containing exactly one `FExecFuncEntry`. Builds an `OriginalToRender` index map.

2. **Render edge derivation** — maps original `FExecFlowEdge`s to render-level edges (`last expanded From` → `first expanded To`). Also adds sequential internal edges within each expanded group (`Expanded[i]` → `Expanded[i+1]`).

3. **Column gathering** — puts render group indices into `TMap<int32, TArray<int32>> ColumnGroups` keyed by `DepthColumn`.

4. **Per-column ordering** (two passes):
   - *Pass 1 — priority sort*: for each column (left-to-right) nodes are sorted by: (a) whether they have a parent in the previous column, (b) median rank of parent nodes, (c) route priority (`True/Valid` = 0, `False/Not Valid` = 1, other = 2), (d) original index.
   - *Pass 2 — crossing minimisation*: re-sorts by parent lane median float values to reduce edge crossings.

5. **Lane assignment** — assigns integer lanes to each node using `FindNearestFreeLane` (tries desired lane → ±1 → ±2 …). Desired lane for a node = median lane of its parents. Starting lane for a column is centred: `-(count-1)/2`.

6. **Node creation** — for each render group: creates a `UExecFlowGraphNode`, sets `NodePosX = DepthColumn * 400`, `NodePosY = lane * LaneSpacing - nodeHeight/2`, calls `AllocateDefaultPins`, adds to graph.

7. **Edge wiring** — for each render edge: resolves `RouteToOutputPinName(RouteLabel)` → compact pin name → calls `GetOutputPinForRoute` → `MakeLinkTo` on the input pin of the target node.

**Layout constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `ColSpacing` | 400 px | Horizontal gap between depth columns |
| `GroupSpacing` | 24 px | Extra vertical padding between lanes |
| `HeaderHeight` | 50 px | Title bar height estimate |
| `FuncRowHeight` | 26 px | Height per function row |
| `BodyPadding` | 20 px | Inner padding |
| `MinNodeHeight` | 80 px | Minimum node height |

`LaneSpacing = MaxNodeHeight + GroupSpacing` where `MaxNodeHeight` is the tallest node across the whole graph.

---

## 7. Slate Widget: `SExecFlowGraphNode`

**Files:** `SExecFlowGraphNode.h` / `SExecFlowGraphNode.cpp`

Custom `SGraphNode` subclass. Registered as a factory so `SGraphEditor` uses it automatically for every `UExecFlowGraphNode` it encounters.

### `SExecFlowGraphNode`

Built with `SLATE_BEGIN_ARGS` (no custom arguments). Constructed with a pointer to the owning `UExecFlowGraphNode`.

**`UpdateGraphNode()`** rebuilds the widget tree:

```
SHorizontalBox
├── LeftNodeBox   (SVerticalBox) ← input exec pin
├── SBorder (node body)
│   └── SVerticalBox
│       ├── SBorder (title bar)   ← colored with GetNodeTitleColor(), shows BlueprintName
│       └── SBorder (card area)   ← dark inner box
│           └── SVerticalBox
│               └── [per-entry] SButton wrapping BuildFuncRow()
└── RightNodeBox  (SVerticalBox) ← output exec pin(s)
```

After rebuilding the tree, `SGraphNode::CreatePinWidgets()` is called to populate `LeftNodeBox`/`RightNodeBox` with actual pin widgets (which then appear in the correct rails).

### `BuildFuncRow(const FExecFuncEntry& Entry)`

Returns a `SButton` (with `NoBorder` style, hand cursor) containing:

- **Kind badge** (`[Ev]`, `[CE]`, `[ED]`, `[M]`, `[S]`, `[F]`) — small gray text.
- **Function name** — larger bold/regular/italic text depending on kind; colour varies:
  - Root node entry → yellow-gold
  - Cycle truncated → warm orange + `(!) ` prefix
  - EventDispatcher → cyan
  - Macro → dimmed gray italic
  - ExecStep → light gray
  - Default → near-white bold

**Click handler:** calls `FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node)` to navigate the Blueprint Editor to the source node. Falls back to `BringKismetToFocusAttentionOnObject(BP)` if `SourceNode` is stale.

### `FExecFlowGraphNodeFactory`

Implements `FGraphPanelNodeFactory`. `CreateNode` casts the incoming `UEdGraphNode*` to `UExecFlowGraphNode*`; if successful returns `SNew(SExecFlowGraphNode, ExecNode)`, otherwise returns `nullptr` (letting the default factory handle it).

Registered in `FBPExecFlowViewerModule::StartupModule` via `FEdGraphUtilities::RegisterVisualNodeFactory`, unregistered in `ShutdownModule`.

---

## 8. Slate Widget: `SExecLocalPathWidget`

**Files:** `SExecLocalPathWidget.h` / `SExecLocalPathWidget.cpp`

The root content widget that lives inside the nomad docking tab. Implements **`FGCObject`** to keep the `UExecFlowGraph` alive (since it is a `UObject` but not owned by any `UPROPERTY` chain reachable from GC roots).

### Members

| Member | Type | Purpose |
|--------|------|---------|
| `FlowGraph` | `TObjectPtr<UExecFlowGraph>` | The live graph object; kept alive via `AddReferencedObjects` |
| `GraphContainer` | `TSharedPtr<SBox>` | Outer container; swapped to hold the new `SGraphEditor` on each rebuild |
| `GraphEditor` | `TSharedPtr<SGraphEditor>` | Current graph editor widget |
| `TargetNode` | `TWeakObjectPtr<UEdGraphNode>` | The node whose flow is being displayed |
| `ErrorText` | `FText` | Displayed in the error strip when something fails |
| `bHasError` | `bool` | Controls error strip visibility |
| `ForwardDepth` | `int32` | Defaults to 4 |
| `BackwardDepth` | `int32` | Defaults to 2 |

### Layout (built in `Construct`)

```
SVerticalBox
├── Control bar (AutoHeight)
│   └── SHorizontalBox
│       ├── "Backward Depth" label + SSpinBox<int32> [0–12]
│       ├── "Forward Depth"  label + SSpinBox<int32> [0–12]
│       └── "Rebuild" SButton
├── Graph area (FillHeight 1.0)
│   └── SBox (GraphContainer) ← holds SGraphEditor
└── Error strip (AutoHeight, Collapsed unless bHasError)
    └── SBorder + STextBlock (salmon-colored, word-wrapped)
```

### `SetTargetNode(UEdGraphNode* InNode)`

Public API called by the module after it opens the tab. Stores the node and immediately calls `Rebuild()`.

### `Rebuild()`

1. Clears error state.
2. Resolves `TargetNode.Get()` — shows error if stale.
3. Calls `FCrossBPExecTracer::TraceFromNode(Node, BackwardDepth, ForwardDepth)`.
4. Calls `UExecFlowGraph::PopulateGraph(FlowGraph, FlowMap)`.
5. Replaces `GraphContainer`'s content with a fresh `SGraphEditor` via `CreateGraphEditorWidget()`.
6. Calls `PostProcessWithGraphEditor()`.

### `PostProcessWithGraphEditor()`

After the graph editor is created, straightens all connections for readability:
1. Iterates all output pins of all nodes → calls `GraphEditor->StraightenConnections(OutPin, LinkedPin)` per linked pair.
2. Selects all nodes → calls `GraphEditor->OnStraightenConnections()` for a global pass.
3. Clears selection, notifies graph changed.

### `FGCObject` implementation

```cpp
void AddReferencedObjects(FReferenceCollector& Collector) override
{
    if (FlowGraph) Collector.AddReferencedObject(FlowGraph);
}
FString GetReferencerName() const override { return TEXT("SExecLocalPathWidget"); }
```

This is **critical** — without it, GC can collect `FlowGraph` between frames since no `UPROPERTY` holds it.

---

## 9. Module Entry Point (`FBPExecFlowViewerModule`)

**Files:** `BPExecFlowViewer.h` (Public) / `BPExecFlowViewer.cpp` (Private)

Implements `IModuleInterface`. The module entry macro at the bottom of the `.cpp`:
```cpp
IMPLEMENT_MODULE(FBPExecFlowViewerModule, BPExecFlowViewer)
```

### `StartupModule()`

1. Creates `GExecLocalNodeFactory` (a `TSharedPtr<FExecFlowGraphNodeFactory>`) and registers it via `FEdGraphUtilities::RegisterVisualNodeFactory`.
2. Registers a **nomad tab spawner** named `"BPExecFlowViewer"` (display: "Exec Flow", hidden from menu).
3. Defers **context-menu extension** registration via `UToolMenus::RegisterStartupCallback` → `RegisterMenuExtensions`.

### `RegisterMenuExtensions()`

Extends six `UToolMenu` names (the per-node-class context menus fired in the Blueprint Editor):

```
GraphEditor.GraphNodeContextMenu.K2Node_CallFunction
GraphEditor.GraphNodeContextMenu.K2Node_Event
GraphEditor.GraphNodeContextMenu.K2Node_CustomEvent
GraphEditor.GraphNodeContextMenu.K2Node_FunctionEntry
GraphEditor.GraphNodeContextMenu.K2Node_MacroInstance
GraphEditor.GraphContextMenu.Common          ← generic fallback
```

Each gets a `AddDynamicSection` that calls `BuildContextMenuSection`.

### `BuildContextMenuSection(UToolMenu* Menu)`

Attempts to resolve the right-clicked node from:
1. `UGraphNodeContextMenuContext` found via `Menu->FindContext<>()`.
2. `GEditor->GetSelectedObjects()` as a fallback.

Validates the node has at least one exec pin (`IsValidFlowNode`). If valid, adds a separator and a "View Exec Flow" menu entry that calls `TriggerLocalFlowView`.

Uses `FindOrAddNodeActionsSection` to find an existing actions section by trying five candidate section names before adding a new one.

### `TriggerLocalFlowView(UEdGraphNode* Node)`

1. `FGlobalTabmanager::Get()->TryInvokeTab(ExecFlowTabName)` — opens or focuses the tab.
2. Pins `ViewerWidgetPtr` (a `TWeakPtr<SExecLocalPathWidget>`) → calls `Widget->SetTargetNode(Node)`.

### `ShutdownModule()`

Unregisters the node factory, unregisters tool menus callbacks/owner, unregisters the tab spawner.

### Module-level statics

```cpp
static const FName ExecFlowTabName("BPExecFlowViewer");
static TSharedPtr<FExecFlowGraphNodeFactory> GExecLocalNodeFactory;
```

`GExecLocalNodeFactory` is file-local to avoid exposing it.

---

## 10. Full Data-Flow Pipeline

Here is the complete sequence from user input to rendered widget:

```
1. User right-clicks a Blueprint node in the Blueprint Editor
        │
2. UToolMenus fires the dynamic section for the matching K2Node context menu
        │
3. BuildContextMenuSection resolves UEdGraphNode* from UGraphNodeContextMenuContext
        │
4. User clicks "View Exec Flow"
        │
5. TriggerLocalFlowView(Node):
   a) TryInvokeTab("BPExecFlowViewer")          → OnSpawnTab() creates SExecLocalPathWidget
   b) SExecLocalPathWidget::SetTargetNode(Node)
        │
6. SetTargetNode → Rebuild()
        │
7. FCrossBPExecTracer::TraceFromNode(Node, BackwardDepth, ForwardDepth)
   - BFS downstream (EGPD_Output exec pins)
   - BFS upstream   (EGPD_Input  exec pins)
   - Collects FEdgeRecord{From, To, NormalizedRoute}
   - Collects FNodeMeta{Depth} per visited node
   - Resolves BlueprintName per node
   - Collects OutgoingRouteLabels from ALL exec output pins
   → returns FExecFlowMap { Groups[], Edges[], RootGroupIndex }
        │
8. UExecFlowGraph::PopulateGraph(FlowGraph, FlowMap)
   - Expands multi-entry groups (1 entry → 1 render node)
   - Derives render edges (original edge indices → render indices)
   - Adds intra-group sequential edges
   - Sorts columns: priority sort + crossing minimisation
   - Assigns lanes via FindNearestFreeLane
   - Creates UExecFlowGraphNode objects with pin allocation and positions
   - Wires pins with MakeLinkTo
   → FlowGraph is now a fully-populated UEdGraph
        │
9. SExecLocalPathWidget::CreateGraphEditorWidget()
   - Creates SGraphEditor with FlowGraph as EditGraph
   - FExecFlowGraphNodeFactory::CreateNode() is called per node
     → returns SExecFlowGraphNode(ExecNode) for each UExecFlowGraphNode
        │
10. SExecLocalPathWidget::PostProcessWithGraphEditor()
    - StraightenConnections per edge pair
    - OnStraightenConnections globally
    - NotifyGraphChanged
        │
11. SGraphEditor renders the final interactive flow diagram
    - User can zoom/pan
    - Clicking a function row → FKismetEditorUtilities::BringKismetToFocusAttentionOnObject
```

---

## 11. Module: BPHealthAnalyzer

This is a **separate concern** from BPExecFlowViewer. It performs static analysis of Blueprint assets:

### `FBPAnalyzerLogic` (pure static class)

```cpp
static TArray<FBPIssue> AnalyzeAllBlueprints();
static TArray<FBPIssue> AnalyzeBlueprint(UBlueprint* BP, const TSet<FString>& CrossBPCallSet);
```

Checks performed per Blueprint:
- `CheckDisconnectedExecPins` — nodes with unconnected exec pins
- `CheckUnusedVariables` — variables defined but never referenced
- `CheckUnusedFunctions` — functions defined but never called (cross-BP call set passed in for accuracy)
- `CheckNodesWithNoOutputConnections` — nodes whose all exec outputs are disconnected

### `FBPIssue` (struct)

| Field | Description |
|-------|-------------|
| `BlueprintName` | Name of the Blueprint asset |
| `GraphName` | Name of the graph (EventGraph, function graph, etc.) |
| `NodeName` | Human-readable node name |
| `IssueDescription` | What is wrong |
| `Severity` | `0` = warning, `1` = error |
| `NodeGuid` | `FGuid` identifying the specific node instance |
| `SourceBlueprint` | `TWeakObjectPtr<UBlueprint>` |
| `SourceNode` | `TWeakObjectPtr<UEdGraphNode>` |

### Entry points

- Opened via: **Window → BPHealthAnalyzer** or the toolbar button (registered in `RegisterMenus`).
- Tab name: `"BPHealthAnalyzer"`.
- Style: `FBPHealthAnalyzerStyle` (loads icon from `Resources/Icon128.png`).
- Commands: `FBPHealthAnalyzerCommands` → `OpenPluginWindow`.

---

## 12. Build Configuration

### `BPExecFlowViewer.Build.cs`

All dependencies are **private** (nothing is exposed publicly):

```
Core, CoreUObject, Engine, UnrealEd
BlueprintGraph         ← UK2Node_*, EdGraph types
GraphEditor            ← SGraphEditor, FGraphPanelNodeFactory
Kismet                 ← FKismetEditorUtilities, KismetDebugUtilities
AssetRegistry          ← for potential BP scanning
Slate, SlateCore        ← all Slate widgets
InputCore              ← FKey, keyboard input
ApplicationCore        ← FSlateApplication
ToolMenus              ← UToolMenus, UToolMenu, FToolMenuSection
Projects               ← IPluginManager (if needed)
```

### `BPHealthAnalyzer.Build.cs`

`Core` is public; everything else private. Shares most of the same dependencies.

### PCH

Both modules use `UseExplicitOrSharedPCHs`.

### Compilation mode

Use **LiveCoding** (`Ctrl+Alt+F11`) for `.cpp`-only changes. Full recompile with UBT if any `.h` changes.

---

## 13. Key UE5 APIs Used

| API | Where Used | Purpose |
|-----|-----------|---------|
| `FEdGraphUtilities::RegisterVisualNodeFactory` | `StartupModule` | Hook custom SGraphNode factory into the graph editor |
| `FGlobalTabmanager::RegisterNomadTabSpawner` | `StartupModule` | Register the "Exec Flow" docking tab |
| `UToolMenus::RegisterStartupCallback` | `StartupModule` | Defer menu registration until ToolMenus is ready |
| `UToolMenus::Get()->ExtendMenu(Name)` | `RegisterMenuExtensions` | Extend Blueprint node context menus |
| `Menu->AddDynamicSection` | `RegisterMenuExtensions` | Dynamic section that runs at menu open time |
| `Menu->FindContext<UGraphNodeContextMenuContext>` | `BuildContextMenuSection` | Get the right-clicked node from context |
| `UEdGraphPin::PinType.PinCategory == PC_Exec` | `IsValidFlowNode`, tracer | Identify exec pins |
| `Pin->GetDisplayName()` | Tracer | Get readable pin names (handles renamed UE pins) |
| `UK2Node_CallFunction::GetTargetFunction` | Tracer | Resolve the called function for BP attribution |
| `UEdGraph::GetTypedOuter<UBlueprint>()` | Tracer | Walk up the outer chain to the owning Blueprint |
| `NewObject<T>(Outer)` | Graph layer | Create UObject graph nodes |
| `UEdGraphPin::MakeLinkTo` | `PopulateGraph` | Wire graph node pins |
| `SGraphNode::CreatePinWidgets()` | `SExecFlowGraphNode` | Spawn Slate pin widgets for the node |
| `FKismetEditorUtilities::BringKismetToFocusAttentionOnObject` | `SExecFlowGraphNode` | Click-to-navigate |
| `FGCObject::AddReferencedObjects` | `SExecLocalPathWidget` | Prevent GC from collecting FlowGraph |
| `SGraphEditor::StraightenConnections` | `PostProcessWithGraphEditor` | Auto-straighten wires |
| `SGraphEditor::OnStraightenConnections` | `PostProcessWithGraphEditor` | Global straighten pass |

---

## 14. Known Limitations & What Is Not Yet Implemented

### Cross-Blueprint traversal

Despite being named `CrossBPExecTracer`, the tracer **does not** follow `UK2Node_CallFunction` edges into other Blueprint graphs. When a call goes to another Blueprint, the tracer assigns the target Blueprint's name to the node, but the traversal stops at the same graph boundary (`Node->GetGraph() != RootGraph` check). This is the biggest planned feature gap.

### Runtime / live execution tracing

The viewer is entirely **static** — it reads the Editor graph topology at design time. It does **not** hook into `FBlueprintCoreDelegates`, `FKismetDebugUtilities`, or the Blueprint VM. There is no live highlighting of which node is currently executing.

### Cycle detection

Cycle detection (`bIsCycleTruncated`) is tracked via `BestDepth` but the flag is **never actually set** in the current tracer code. The field exists in `FExecFuncEntry` but the tracer does not set it to `true`.

### `IntraGraphExecPath`

`FExecFuncEntry::IntraGraphExecPath` is declared and serialised into the tooltip, but the tracer never populates it. It is reserved for a future "exec path string" feature.

### `EExecNodeKind::EventDispatcher`

The `EventDispatcher` kind value exists in the enum and the display code has a special colour for it, but the tracer's `GetKind()` function never returns it (there is no `IsA<UK2Node_CreateDelegate>` or similar check).

### `ExecMapBuilder` (orphaned files)

`ExecMapBuilder.cpp/.h` appears in the `BPHealthAnalyzer` module's intermediate build artifacts but is **not present** in the current source tree. These are stale build artifacts from a previous iteration.

### `SExecFlowViewerWidget`, `ExecLocalGraph`, `ExecLocalGraphData`, `ExecLocalPath`

Also appear only in BPHealthAnalyzer intermediate artifacts — these were earlier iterations that have been superseded by the files now in `BPExecFlowViewer/Private/`.

### Tab focus race

`TriggerLocalFlowView` calls `TryInvokeTab` which may create the tab asynchronously, then immediately calls `SetTargetNode`. If the tab was newly created, `ViewerWidgetPtr` may not yet be valid. In practice the first "View Exec Flow" on a fresh editor will silently do nothing, and the user must right-click again. Needs a deferred callback.

### No zoom-to-fit on rebuild

After `Rebuild()` the graph editor does not call `ZoomToFit()` or `JumpToNode(root)`, so the view may remain wherever it was previously.

---

## 15. Planned Next Steps / Where to Continue

Below are the logical next steps roughly in priority order:

### 1. Fix the tab-creation race condition

In `TriggerLocalFlowView`, after `TryInvokeTab` returns, check if `ViewerWidgetPtr` is valid. If not, store the pending node and apply it in `OnSpawnTab` after widget creation.

```cpp
// In OnSpawnTab, after creating the widget:
if (PendingNode.IsValid())
{
    Widget->SetTargetNode(PendingNode.Get());
    PendingNode = nullptr;
}
```

### 2. Zoom-to-fit / jump-to-root after rebuild

In `PostProcessWithGraphEditor`, after straightening:
```cpp
UExecFlowGraphNode* RootNode = // find node where GroupData.DepthColumn == 0 && Functions[0].bIsRoot
GraphEditor->JumpToNode(RootNode, /*bRequestRename=*/false);
// or: GraphEditor->ZoomToFit();
```

### 3. Implement cycle detection

In `CrossBPExecTracer::TraceFromNode`, when the BFS encounters a node already in `BestDepth` with a shallower existing depth, mark the entry's `bIsCycleTruncated = true` instead of silently skipping.

### 4. Set `EExecNodeKind::EventDispatcher`

In `GetKind()`, add a check for `UK2Node_CreateDelegate` or `UK2Node_CallDelegate`:
```cpp
if (Node->IsA<UK2Node_CallDelegate>()) return EExecNodeKind::EventDispatcher;
```

### 5. Cross-Blueprint traversal

In the downstream BFS, when visiting a `UK2Node_CallFunction`:
- Resolve `CallFunctionNode->GetTargetFunction()->GetOwnerClass()`.
- Use `FBlueprintEditorUtils::FindBlueprintForClass` to find the target Blueprint.
- Find the function graph inside it using `FBlueprintEditorUtils::FindFunctionGraph`.
- Continue BFS into the new graph, assigning a deeper positive depth.

This requires:
- Visiting multiple `UEdGraph*` objects, not just `RootGraph`.
- Updating the graph-boundary check in the tracer.
- Possibly breaking the visited-set to be graph-aware to handle same-named functions in different BPs.

### 6. Live execution tracing hook

Hook `FBlueprintCoreDelegates::OnInstrumentScriptEvent` or `FKismetDebugUtilities::OnScriptException` to highlight which node in the flow graph was last executed during PIE.

### 7. Improve `SExecLocalPathWidget` UX

- Add a "Lock" button to keep the current graph from refreshing when selection changes.
- Show the traced Blueprint name and node name in the panel header.
- Persist `ForwardDepth` / `BackwardDepth` to `EditorPerProjectUserSettings`.

### 8. Clean up orphaned BPHealthAnalyzer files

Remove or properly integrate the outdated copies of `ExecFlowGraph`, `SExecFlowGraphNode`, `SExecFlowViewerWidget`, `ExecLocalGraph*`, `ExecLocalPath*`, `ExecMapBuilder`, `ExecFlowViewerIntegration` that live in `BPHealthAnalyzer/Private/`.

---

## 16. Coding Conventions Enforced in This Plugin

| Rule | Enforcement |
|------|------------|
| No `std::` containers | Use `TArray`, `TMap`, `TSet`, `TQueue` |
| No `std::shared_ptr` / `std::unique_ptr` | Use `TSharedPtr`, `TSharedRef`, `MakeShared<>` |
| No `new` / `delete` for UObjects | Use `NewObject<T>()`, rely on GC |
| No raw owning pointers | `TWeakObjectPtr` for observed UObjects, `TSharedPtr` for Slate |
| UCLASS/USTRUCT must have `GENERATED_BODY()` | — |
| `#include "FileName.generated.h"` must be last include | — |
| Slate widgets: `S` prefix, `SLATE_BEGIN_ARGS` | `SExecFlowGraphNode`, `SExecLocalPathWidget` |
| Class prefixes: `F` for non-UObject, `U` for UObject, `S` for Slate | — |
| Bool members: `bIsEnabled` naming | `bIsRoot`, `bHasError`, `bIsSynthetic`, etc. |
| `UE_LOG` with module-specific log category | Use `LogBPExecFlowViewer` when adding log statements |
| Editor-only code inside `#if WITH_EDITOR` if in shared `.h` | — |
| Guard UObject pointers with `IsValid()` before dereference | — |
| Use `TObjectPtr<T>` for member UObject pointers | `TObjectPtr<UExecFlowGraph> FlowGraph` |
| Use `ensureMsgf()` over `check()` in editor plugin code | Avoids hard crashes in editor |

