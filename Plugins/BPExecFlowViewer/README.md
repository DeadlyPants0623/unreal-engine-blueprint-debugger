# Blueprint Exec Flow Viewer

> **Unreal Engine 5.7** · Editor-only plugin · v1.0.0

Visualize **upstream and downstream Blueprint execution flow** from any node — in one dockable, read-only graph. Right-click a node in the Blueprint Editor and choose **View Exec Flow**.

**Get it:** [GitHub Releases](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases) · [Fab](https://www.fab.com/) · [Issues / support](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues)

---

## Features

- **Exec-flow map** — callers (left), root (centre), callees (right), with route labels (`Branch: True`, `IsValid: Valid`, `Exec`, …)
- **Cross-Blueprint tracing** — follows `CallFunction` into other Blueprint assets when resolvable via the Asset Registry
- **Click-to-jump** — click any row to open the source node in the Blueprint Editor
- **Re-root (→)** — click **→** on a row to re-trace with that step as the new centre
- **Depth controls** — backward and forward depth **0–32** (defaults: 2 / 4)
- **Zoom-to-fit** — graph frames all nodes after each rebuild

---

## Installation

Requires **UE 5.7** and a **C++ project** (or a one-time “Add C++ Class” so the editor can compile plugins).

### From GitHub Releases (recommended)

1. Open [Releases](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases) and download the latest **`Blueprint-Exec-Flow-Viewer-*.zip`**.
2. Create `YourProject/Plugins/BPExecFlowViewer/` if needed, then extract the zip **into** that folder (you should see `BPExecFlowViewer.uplugin` there). Do **not** nest an extra folder level.
3. Open the project in Unreal Engine 5.7.
4. **Edit → Plugins** → search **Blueprint Exec Flow Viewer** → **Enable** → restart when prompted.
5. Let the editor compile the plugin on first enable.

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

---

## FAQ

### How do I use this plugin?

1. Enable **Blueprint Exec Flow Viewer** under **Edit → Plugins** (UE **5.7**), restart if prompted, and let the editor compile on first use if you installed from source.
2. Open any Blueprint in the **Blueprint Editor**.
3. **Right-click** a node on the exec chain (e.g. a function call or event) and choose **View Exec Flow**.
4. The **Exec Flow** nomad tab opens with your node in the **centre** (gold), callers on the **left** (blue), and callees on the **right** (green).
5. Adjust **Backward Depth** / **Forward Depth** (defaults **2** / **4**), then click **Rebuild** to widen or narrow the trace.
6. **Click any row** in a card to jump to that node in the Blueprint Editor, or click **→** on a row to re-trace from that step as the new root.

See [Usage](#usage) and [Reading the graph](#reading-the-graph) above for panel details and badge meanings.

### Why don’t I see **View Exec Flow** when I right-click a node?

The menu entry only appears on nodes that participate in execution flow — typically nodes with **Exec** pins (function calls, events, macros, and similar). Pure data nodes (variables, math, getters) have no exec path to trace, so the option is omitted. If you expected a node to qualify, try right-clicking a connected **Exec** pin or a `CallFunction` / `Event` node upstream or downstream of it.

### Does this trace runtime execution during Play-In-Editor?

No. The viewer analyzes **static graph topology** in the editor. It shows which nodes *can* run before and after your selection along exec wires, not what actually ran in a given PIE session. For runtime debugging you still need breakpoints, logging, or a dedicated runtime tracer.

### Why is a cross-Blueprint function missing from the graph?

Cross-Blueprint hops rely on the **Asset Registry** and a resolvable target Blueprint (usually after the callee has been compiled at least once). Renamed assets, broken references, interface-only calls, or Blueprints not yet indexed can leave a gap. Increase **Forward Depth** and click **Rebuild**; if the callee still does not appear, open that Blueprint, compile it, then trace again from your root node.

### Do I need a C++ project? The plugin won’t enable or compile.

Yes: Unreal must compile the editor module, which requires a **C++ project** (or adding any C++ class once so UBT generates project files). Blueprint-only projects cannot build third-party editor plugins from source. After installing from GitHub Releases or Fab, enable the plugin under **Edit → Plugins** and restart when prompted.

---

## License

MIT — see [LICENSE](LICENSE).

## Support

Questions and bugs: [GitHub Issues](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues).
