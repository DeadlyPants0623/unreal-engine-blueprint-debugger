# Unreal Engine Blueprint Debugger

Editor plugin for **Unreal Engine 5.7** — visualize Blueprint execution flow from any graph node.

## Repository layout

This repository contains **only** the plugin:

```
Plugins/BPExecFlowViewer/
```

The game project files in a parent folder (if present) are **not** versioned.

## Installation

### Option A — GitHub Release (recommended)

1. Open **[Releases](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases)** and pick the latest version.
2. Download **`BPExecFlowViewer-X.Y.Z-source.zip`** (or **`…-Win64.zip`** if you want a prebuilt editor binary and one is published for your platform).
3. Extract the zip so the layout is:

   ```
   YourProject/
     Plugins/
       BPExecFlowViewer/
         BPExecFlowViewer.uplugin
         Source/
         Resources/
         ...
   ```

4. Open your project in **Unreal Engine 5.7** (must be a **C++ project**, or add any C++ class once so the editor generates project files).
5. Go to **Edit → Plugins**, search **Blueprint Exec Flow Viewer**, enable it, and **restart** the editor.
6. On first launch after enabling, allow the editor to **compile** the plugin (source zip) or load the prebuilt module (Win64 zip).

### Option B — Clone with Git

From your Unreal project root:

```bash
git clone https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger.git Plugins/BPExecFlowViewer
```

Then follow steps 4–6 above.

## Usage

See the plugin README: [`Plugins/BPExecFlowViewer/README.md`](Plugins/BPExecFlowViewer/README.md)

## Support

[GitHub Issues](https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues)

## License

MIT — see [`Plugins/BPExecFlowViewer/LICENSE`](Plugins/BPExecFlowViewer/LICENSE).

## Maintainers — creating a release

Push a version tag to run the release workflow:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The workflow uploads a **source zip** on every tag. A **Win64 prebuilt zip** is optional and requires two setup steps on your PC (see below).

### Optional: Win64 prebuilt releases (your PC as the build machine)

GitHub’s cloud runners do **not** have your `C:\Program Files\Epic Games\UE_5.7` install. To build the Win64 zip in CI, use your own Windows machine as a **self-hosted runner**.

**1. Add the `UE_ROOT` secret** (repo on GitHub):

1. Open https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger  
2. **Settings** → **Secrets and variables** → **Actions**  
3. **New repository secret**  
4. Name: `UE_ROOT`  
5. Value: `C:\Program Files\Epic Games\UE_5.7`  
6. **Add secret**

**2. Register this PC as a self-hosted runner** (same repo):

1. **Settings** → **Actions** → **Runners** → **New self-hosted runner**  
2. Choose **Windows** and follow the commands (download, configure, run `run.cmd` in the runner folder).  
3. Leave the runner app running (or install it as a service) so jobs can start when you push a tag.

The Win64 job uses `runs-on: self-hosted`, so it only runs on that machine — where `UE_ROOT` must point to your real UE install.

If you skip step 2, releases still publish the **source zip** only (enough for most users).
