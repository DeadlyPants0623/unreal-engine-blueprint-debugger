# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **Unreal Engine 5.7** project (`MyCity`) with editor plugins under `Plugins/`.

### BPExecFlowViewer (Fab / primary)

Standalone editor plugin: [`Plugins/BPExecFlowViewer/`](Plugins/BPExecFlowViewer/)

- **Module:** `BPExecFlowViewer` (Editor only)
- Blueprint execution flow tracer and graph viewer
- Packaged for Fab; see `README.md` and `CHANGELOG.md` in the plugin folder

### BPHealthAnalyzer (local only)

[`Plugins/BPHealthAnalyzer/`](Plugins/BPHealthAnalyzer/) — toolbar + health analysis scaffold. **Not** included in the Fab zip. Enable separately in `.uproject` if needed.

### Other plugins

- [`Plugins/RiotDiagnostics/`](Plugins/RiotDiagnostics/) — unrelated; never bundle with Fab artifacts

## Build & Compile

Build via Unreal Build Tool from inside UE 5.7 editor (**File → Refresh Visual Studio Project**, then build from VS), or use **Live Coding** (Ctrl+Alt+F11) for `.cpp`-only changes. Full rebuild is required when any `.h` changes.

- If UBT errors mention "missing module", add it to `BPExecFlowViewer.Build.cs` under `PrivateDependencyModuleNames`
- Key dependencies: `Kismet`, `KismetCompiler`, `BlueprintGraph`, `GraphEditor`, `UnrealEd`, `Slate`, `SlateCore`, `ToolMenus`, `AssetRegistry`
- Module type is `Editor` — never referenced in game builds
- Fab package: `Scripts/PackageForFab.bat` (set `UE_ROOT`)

## Architecture

### Data flow

```
Right-click node in Blueprint Editor
  → FBPExecFlowViewerModule::BuildContextMenuSection (BPExecFlowViewer.cpp)
  → TriggerLocalFlowView(UEdGraphNode*)  [PendingTargetNode if tab not yet spawned]
  → SExecLocalPathWidget::SetTargetNode
  → FCrossBPExecTracer::TraceFromNode  →  FExecFlowMap
  → UExecFlowGraph::PopulateGraph(FExecFlowMap)
  → SGraphEditor renders UExecFlowGraphNode widgets
  → SExecFlowGraphNode::BuildFuncRow (clickable rows → jump to source node)
```

### Core types (`ExecFlowTypes.h`)

- `FExecFuncEntry` — one row in a card: node name, kind badge, root flag, outgoing route labels
- `FExecBPGroup` — one card/column in the graph: a Blueprint container holding N `FExecFuncEntry` rows
- `FExecFlowEdge` — a directed edge between two group indices, carrying a route label (`"Branch: True"`, `"IsValid: Valid"`, `"Exec"`, etc.)
- `FExecFlowMap` — the full result: `TArray<FExecBPGroup>`, `TArray<FExecFlowEdge>`, root index

### Key classes

| Class | File | Role |
|---|---|---|
| `FCrossBPExecTracer` | `CrossBPExecTracer.h/.cpp` | Static tracer — walks exec pins forward/backward, produces `FExecFlowMap` |
| `UExecFlowGraph` / `UExecFlowGraphNode` | `ExecFlowGraph.h/.cpp` | UEdGraph subclasses; `PopulateGraph` lays out columns (X = depth × 400) |
| `UExecFlowGraphSchema` | `ExecFlowGraph.h` | Read-only schema — rejects all user connections |
| `SExecLocalPathWidget` | `SExecLocalPathWidget.h/.cpp` | Main dockable panel: depth spinners, Rebuild, `SGraphEditor`, cluster overlay |
| `SExecFlowGraphNode` | `SExecFlowGraphNode.h/.cpp` | Custom `SGraphNode` — title bar + clickable function rows + causality ◈ |
| `SExecFlowClusterOverlay` | `SExecFlowClusterOverlay.h/.cpp` | Blueprint-group background rectangles in graph-space |
| `FBPExecFlowViewerModule` | `BPExecFlowViewer.h/.cpp` | Module entry: tab spawner, context menu, `PendingTargetNode` |
| `FCausalityAnalyzer` | `CausalityAnalyzer.h/.cpp` | Data-edge causality chain for ◈ highlight |

### Graph layout

Columns at `X = DepthColumn * 400`. Callers (depth < 0) left; root (0) centre; callees (depth > 0) right. Cycles skipped — `bIsCycleTruncated` on `FExecFuncEntry`.

## C++ Conventions

- UE smart pointers only: `TSharedPtr/Ref`, `TWeakPtr`, `TWeakObjectPtr`, `MakeShared<T>()`, `MakeUnique<T>()`
- UE containers only: `TArray`, `TMap`, `TSet` — never `std::` equivalents
- `NewObject<T>()` for UObjects; never `new`/`delete` a UObject
- Always null-check `Cast<T>` before dereferencing
- Use `ensureMsgf()` over `check()` — crashes the editor less aggressively
- Use `UE_LOG` with a custom log category; consider `FMessageLog` for user-visible errors
- Target **UE 5.7**
- Prefer delegate/callback hooks over polling in `Tick()`
- Editor-only logic inside `#if WITH_EDITOR` when needed

## Naming

- `F` prefix: structs and non-UObject classes
- `U` prefix: UObject subclasses
- `S` prefix: Slate widgets
- `bIsSomething` for bools
- `OnEventName` for delegates
