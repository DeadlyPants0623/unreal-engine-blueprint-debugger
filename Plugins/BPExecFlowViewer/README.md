# Blueprint Exec Flow Viewer

> **Unreal Engine 5.7** · Editor-only plugin · v1.0.0

Visualize **upstream and downstream Blueprint execution flow** from any node — in one dockable, read-only graph. Right-click a node in the Blueprint Editor and choose **View Exec Flow**.

**Get it:** [Fab](https://www.fab.com/) (search *Blueprint Exec Flow Viewer*) · [GitHub](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger) · [Issues / support](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues)

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

### From GitHub

```bash
git clone https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger.git Plugins/BPExecFlowViewer
```

Or download a release and extract to `YourProject/Plugins/BPExecFlowViewer/` (no `Binaries/` or `Intermediate/`).
2. Open the project in **Unreal Engine 5.7** (C++ project, or add a C++ class so the editor compiles plugins).
3. **Edit → Plugins** → search **Blueprint Exec Flow Viewer** → enable → restart.
4. Let the editor compile the plugin on first launch.

### From Fab

Install via the Fab library into your project, then enable the plugin and restart the editor. Same UE **5.7** and C++ compile requirements apply.

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
