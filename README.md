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

**2. Use one self-hosted runner for multiple repos (organization)**

Repo-level runners (e.g. only `AdvancedAIDemo`) do **not** see jobs from other repositories. To use `C:\actions-runner` for **both** this plugin and another repo, register it once at the **GitHub organization** level:

1. Create or use a GitHub **Organization** that owns both repositories (Settings → transfer repo, or create the org and add repos).
2. Org → **Settings** → **Actions** → **Runners** → **New self-hosted runner** → Windows.
3. On your PC (reuse existing install):

   ```powershell
   cd C:\actions-runner
   .\config.cmd remove    # disconnects the old repo-only registration
   .\config.cmd --url https://github.com/YOUR_ORG_NAME --token PASTE_TOKEN_FROM_GITHUB
   .\run.cmd
   ```

4. In the org runner settings, allow access to **unreal-engine-blueprint-debugger** and **AdvancedAIDemo** (or “All repositories”).
5. Set secret **`UE_ROOT`** = `C:\Program Files\Epic Games\UE_5.7` on **each** repo that runs the Win64 release job (repo → Settings → Secrets → Actions).

The Win64 job uses `runs-on: self-hosted`. The runner must be **Online** when you push a version tag.

**No organization?** You can run two registrations on the same PC (e.g. `C:\actions-runner` + `C:\actions-runner-bpexec`) — two `run.cmd` processes — one per repo. That is still one machine, but not one GitHub runner registration.

If you skip runner setup, releases still publish the **source zip** only (enough for most users).
