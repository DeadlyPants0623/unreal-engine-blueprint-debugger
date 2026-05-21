# Blueprint Exec Flow Viewer

> **Unreal Engine 5.7** · Editor-only plugin · v1.0.0

Visualize **upstream and downstream Blueprint execution flow** from any node — in one dockable, read-only graph. Right-click a node in the Blueprint Editor and choose **View Exec Flow**.

**Get it:** [GitHub Releases](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases) · [Fab](https://www.fab.com/) · [Issues / support](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues)

---

## Features

- **Exec-flow map** — callers (left), root (centre), callees (right), with route labels (`Branch: True`, `IsValid: Valid`, `Exec`, …)
- **Cross-Blueprint tracing** — follows `CallFunction` into other Blueprint assets when resolvable via the Asset Registry
- **Causality highlight** — click the **◈** button on a row to dim unrelated nodes and highlight data/exec ancestors; **Clear ◈** resets
- **Click-to-jump** — click any row to open the source node in the Blueprint Editor
- **Depth controls** — backward and forward depth **0–32** (defaults: 2 / 4)
- **Zoom-to-fit** — graph frames all nodes after each rebuild

---

## Installation

Requires **UE 5.7** and a **C++ project** (or a one-time “Add C++ Class” so the editor can compile plugins).

### From GitHub Releases (recommended)

1. Open [Releases](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases) and download the latest **`BPExecFlowViewer-*-source.zip`** (or **`*-Win64.zip`** for a prebuilt Win64 editor build when available).
2. Extract into your project so you have `YourProject/Plugins/BPExecFlowViewer/` (the folder must contain `BPExecFlowViewer.uplugin`). Do **not** nest an extra folder level.
3. Open the project in Unreal Engine 5.7.
4. **Edit → Plugins** → search **Blueprint Exec Flow Viewer** → **Enable** → restart when prompted.
5. Let the editor compile the plugin on first enable (source zip), or use the prebuilt binaries (Win64 zip).

### From Git (clone)

From your Unreal project directory:

```bash
git clone https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger.git Plugins/BPExecFlowViewer
```

Then repeat steps 3–5 above.

### From Fab

Install from the Fab library into your project, enable the plugin, and restart the editor. Same UE **5.7** and C++ requirements apply.

---

## Usage

1. Open a Blueprint in the Blueprint Editor.
2. Right-click any node with execution (`Exec`) pins.
3. Choose **View Exec Flow**.

The **Exec Flow** nomad tab opens (or focuses) and populates on the **first** click.

### Panel controls

| Control | Default | Purpose |
|---------|---------|---------|
| Backward Depth | 2 | Hops upstream toward callers |
| Forward Depth | 4 | Hops downstream toward callees |
| Rebuild | — | Re-trace after changing depth |
| Clear ◈ | — | Clear causality highlight (visible while active) |

---

## Reading the graph

| Column | Meaning |
|--------|---------|
| Left (blue) | Nodes that execute **before** the root |
| Centre (gold) | The node you right-clicked |
| Right (green) | Nodes that execute **after** the root |

**Kind badges:** `[Ev]` Event · `[CE]` Custom Event · `[ED]` Event Dispatcher · `[M]` Macro · `[S]` Exec step · `[F]` Function call

---

## Supported context-menu entry points

- `K2Node_CallFunction`
- `K2Node_Event`
- `K2Node_CustomEvent`
- `K2Node_FunctionEntry`
- `K2Node_MacroInstance`
- Generic graph node menu (fallback)

---

## Limitations

- **Editor only** — static graph topology; no Play-In-Editor runtime trace.
- **Cross-Blueprint** — depends on Asset Registry / compile state; some targets may be missing.
- **Macros, interfaces, latent/async, delegates** — paths may truncate or omit.
- **Cycles** — back-edges are skipped; affected nodes are marked cycle-truncated.
- **Causality ◈** — static data-pin influences, not guaranteed runtime causality.

---

## License

MIT — see [LICENSE](LICENSE).

## Support

Questions and bugs: [GitHub Issues](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues).
