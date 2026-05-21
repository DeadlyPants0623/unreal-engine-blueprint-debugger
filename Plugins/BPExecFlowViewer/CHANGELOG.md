# Changelog

All notable changes to **Blueprint Exec Flow Viewer** are listed here.

## [1.0.0] - 2026-05-21

First public release for **Unreal Engine 5.7** (editor-only plugin).

### Added

- **View Exec Flow** — right-click any Blueprint node with execution pins to open a dockable **Exec Flow** panel
- **Execution flow graph** — read-only map with callers on the left, your selected node in the centre, and callees on the right, including route labels (`Branch: True`, `IsValid: Valid`, `Exec`, and similar)
- **Cross-Blueprint tracing** — follows `CallFunction` into other Blueprint assets when they can be resolved
- **Depth controls** — adjust backward and forward trace depth (0–32; defaults 2 and 4) and **Rebuild** the graph
- **Cluster overlay** — grouped background regions per Blueprint in the graph
- **Re-root on click** — click a row to treat that step as the new centre of the trace
- **Cycle handling** — back-edges are skipped; truncated nodes are marked when a cycle is detected
- **Causality highlight (◈)** — highlight data-flow ancestors of a row; **Clear ◈** resets the view
- **Click-to-jump** — open the source Blueprint node in the editor from any row
- **Zoom-to-fit** — frame the full graph after each rebuild
- Standalone plugin package (source and Win64 prebuilt zips on GitHub Releases; suitable for Fab submission)
- MIT license

### Fixed

- Exec Flow tab populates on the first right-click (no second click required)
- Flow graph cannot be edited accidentally (read-only graph editor)
- Safer null checks in graph node UI (`ensureMsgf` instead of hard `check`)

### Changed

- Context menu entry uses the plugin toolbar icon
