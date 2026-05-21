# Changelog

All notable changes to the **Blueprint Exec Flow Viewer** plugin are documented here.

## [1.0.0] - 2026-05-21

### Added

- Standalone `BPExecFlowViewer` plugin package for Fab distribution
- Cross-Blueprint execution tracing (`FCrossBPExecTracer`)
- Causality chain highlight (◈) with clear control
- Dockable **Exec Flow** panel with depth controls (0–32) and Rebuild
- Read-only flow graph with cluster overlay and click-to-jump
- Context menu **View Exec Flow** on common K2 node types
- `FilterPlugin.ini` for clean marketplace packaging
- MIT license

### Fixed

- First-open tab race: graph populates without a second right-click (`PendingTargetNode`)
- Flow graph editor set non-editable (`IsEditable(false)`)
- `SExecFlowGraphNode` uses `ensureMsgf` instead of `check` on null node

### Changed

- Zoom-to-fit after rebuild
- Context menu entry uses plugin icon brush
