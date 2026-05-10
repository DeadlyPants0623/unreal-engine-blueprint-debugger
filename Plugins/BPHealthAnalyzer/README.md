# BPExecFlowViewer

> Plugin package: **BPHealthAnalyzer** (`BPHealthAnalyzer.uplugin`)  
> Engine: **Unreal Engine 5.7** | Module type: **Editor only** | Version: 1.0  
> Author: CJ

---

## What Is BPExecFlowViewer?

BPExecFlowViewer is an **editor-only Blueprint debugging tool** built into the BPHealthAnalyzer plugin. It lets you instantly visualise the **execution flow** around any node in a Blueprint graph — upstream callers and downstream callees — as an interactive directed graph in a dockable panel.

Instead of manually tracing `Exec` pin connections across a large graph, you right-click any node, choose **"View Exec Flow"**, and get a clean visual map showing:

- Which nodes execute **before** the selected node (upstream / callers)
- Which nodes execute **after** it (downstream / callees)
- Which **branch or route** each connection follows (e.g. `Branch: True`, `Branch: False`, `IsValid: Valid`)
- The **Blueprint owner** of each node in the chain
- Click-to-navigate: clicking any node in the flow view jumps the Blueprint Editor directly to that node

The viewer is **read-only and editor-only** — it reads the static graph topology at design time and does not run any game code.

---

## How to Open It

1. Open any Blueprint in the **Blueprint Editor**.
2. Right-click any node that has execution (`Exec`) pins — functions, events, custom events, macros, branches, etc.
3. In the context menu, click **"View Exec Flow"**.

A dockable panel named **"Exec Flow"** will open (or be focused if already open) showing the flow graph for that node.

> **Tip:** The panel is a standard UE nomad tab — you can drag it to dock anywhere in the editor layout.

---

## Reading the Flow Graph

Each card in the flow graph represents a single Blueprint node. Cards are arranged in **columns by execution depth**:

| Column position | Meaning |
|-----------------|---------|
| Centre (gold) | The node you right-clicked — the **root** |
| Left of centre (blue) | Nodes that execute **before** the root (upstream callers) |
| Right of centre (green) | Nodes that execute **after** the root (downstream callees) |

### Inside a card

Each card shows:
- **Header bar** — the name of the Blueprint the node belongs to, coloured by depth (blue / gold / green).
- **Node rows** — one row per node in the execution chain. Each row shows:
  - A **kind badge**: `[Ev]` Event · `[CE]` Custom Event · `[ED]` Event Dispatcher · `[M]` Macro · `[S]` Exec Step · `[F]` Function Call
  - The **display name** of the node

### Connections between cards

Arrows between cards are labelled with the **execution route** that was followed:

| Label | Meaning |
|-------|---------|
| `Exec` | Plain execution pin (no branch) |
| `Branch: True` | Branch node — true path |
| `Branch: False` | Branch node — false path |
| `IsValid: Valid` | Is Valid node — object was valid |
| `IsValid: Not Valid` | Is Valid node — object was null/invalid |
| *(other)* | The actual pin display name from the Blueprint |

---

## Controlling Depth

At the top of the Exec Flow panel there are two controls:

| Control | Default | What it does |
|---------|---------|--------------|
| **Backward Depth** | 2 | How many hops upstream (towards callers) to trace |
| **Forward Depth** | 4 | How many hops downstream (towards callees) to trace |

Both accept values from **0 to 12**.

After changing either value, click the **Rebuild** button to re-trace the graph from the same node.

---

## Navigating to a Node

Click any **node row** inside a card to jump the Blueprint Editor directly to that source node. The Blueprint Editor will open (if not already open) and pan/focus on the node.

---

## Supported Node Types

BPExecFlowViewer can be opened from the right-click context menu of these node types in the Blueprint Editor:

- `K2Node_CallFunction` — Function calls
- `K2Node_Event` — Events (e.g. BeginPlay, Tick)
- `K2Node_CustomEvent` — Custom Events
- `K2Node_FunctionEntry` — Function graph entry nodes
- `K2Node_MacroInstance` — Macro instances
- Any other node via the **generic node context menu** as a fallback

The viewer will trace exec pins from whichever node you right-clicked, regardless of which specific type it is.

---

## Current Limitations

- **Same-graph only.** The tracer follows exec connections within the current Blueprint graph. It does not follow function calls into other Blueprints or other graphs.
- **Static only.** The viewer reads the editor graph at design time. It does not highlight what is executing during Play-In-Editor (PIE).
- **No zoom-to-fit on open.** After rebuilding, the view stays wherever it was last positioned. Manually zoom to fit with the scroll wheel or the graph editor's built-in controls.
- **First open may require a second right-click.** If the panel was not previously open, the first "View Exec Flow" click opens the tab but may not populate it. Right-click and choose "View Exec Flow" again.

---

## Installation

The BPExecFlowViewer module is bundled inside the **BPHealthAnalyzer** plugin. To enable it:

1. Copy the `Plugins/BPHealthAnalyzer/` folder into your project's `Plugins/` directory.
2. Open your project in Unreal Engine 5.7.
3. Go to **Edit → Plugins**, search for **BPHealthAnalyzer**, and enable it.
4. Restart the editor when prompted.
5. Recompile if you are running from source.

No additional configuration is required.
