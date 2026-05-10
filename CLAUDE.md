# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **Unreal Engine 5.7 C++ Editor Plugin** project (`MyCity`). The plugin package is `BPHealthAnalyzer` and contains two editor-only modules:

- **BPHealthAnalyzer** — toolbar button + dockable health analysis panel (boilerplate scaffold, less active development)
- **BPExecFlowViewer** — the main module: Blueprint execution flow tracer and graph viewer

## Build & Compile

Build via Unreal Build Tool from inside UE 5.7 editor (**File → Refresh Visual Studio Project**, then build from VS), or use **Live Coding** (Ctrl+Alt+F11) for `.cpp`-only changes. Full rebuild is required when any `.h` changes.

- If UBT errors mention "missing module", add it to `BPExecFlowViewer.Build.cs` under `PrivateDependencyModuleNames`
- Key current dependencies: `Kismet`, `KismetCompiler`, `BlueprintGraph`, `GraphEditor`, `UnrealEd`, `Slate`, `SlateCore`, `ToolMenus`, `AssetRegistry`
- Module type is `Editor` — never referenced in game builds

## Architecture

### Data flow

```
Right-click node in Blueprint Editor
  → FBPExecFlowViewerModule::BuildContextMenuSection (BPExecFlowViewer.cpp)
  → TriggerLocalFlowView(UEdGraphNode*)
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
| `UExecFlowGraph` / `UExecFlowGraphNode` | `ExecFlowGraph.h/.cpp` | UEdGraph subclasses that hold the visual graph; `PopulateGraph` lays out columns (X = depth × 400) |
| `UExecFlowGraphSchema` | `ExecFlowGraph.h` | Read-only schema — rejects all user connections |
| `SExecLocalPathWidget` | `SExecLocalPathWidget.h/.cpp` | Main dockable panel: depth spinners, Rebuild button, `SGraphEditor`, cluster overlay |
| `SExecFlowGraphNode` | `SExecFlowGraphNode.h/.cpp` | Custom `SGraphNode` — renders title bar + clickable function rows |
| `SExecFlowClusterOverlay` | `SExecFlowClusterOverlay.h/.cpp` | `SLeafWidget` overlaid on the graph to draw Blueprint-group background rectangles in graph-space |
| `FBPExecFlowViewerModule` | `BPExecFlowViewer.h/.cpp` | Module entry point; registers tab spawner + context menu extensions |

### Graph layout

Columns are at `X = DepthColumn * 400`. Callers (depth < 0) are left of centre; root (depth 0) is centre; callees (depth > 0) are right. Y positioning uses lane-based alignment for straighter paths. Back-edges (cycles) are skipped — the cycle-truncated node gets `bIsCycleTruncated = true` on `FExecFuncEntry`.

## C++ Conventions

- UE smart pointers only: `TSharedPtr/Ref`, `TWeakPtr`, `TWeakObjectPtr`, `MakeShared<T>()`, `MakeUnique<T>()`
- UE containers only: `TArray`, `TMap`, `TSet` — never `std::` equivalents
- `NewObject<T>()` for UObjects; never `new`/`delete` a UObject
- Always null-check `Cast<T>` before dereferencing
- Use `ensureMsgf()` over `check()` — crashes the editor less aggressively
- Use `UE_LOG(LogYourModule, ...)` with a custom log category; consider `FMessageLog` for user-visible errors
- Flag any API deprecated after UE 5.4 — target is 5.7
- Prefer delegate/callback hooks over polling in `Tick()`
- All editor-only logic must be inside `#if WITH_EDITOR` if there's any risk of non-editor inclusion

## Naming

- `F` prefix: structs and non-UObject classes
- `U` prefix: UObject subclasses
- `S` prefix: Slate widgets
- `bIsSomething` for bools
- `OnEventName` for delegates