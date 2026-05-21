# Maintainer notes

## Creating a release

```bash
git tag v1.0.0
git push origin v1.0.0
```

The [release workflow](workflows/release.yml) fires on every `v*` tag and runs four jobs in order:

1. **Resolve version** — strips the `v` prefix from the tag.
2. **Package source zip** — runs on GitHub-hosted Ubuntu; no UE install needed.
3. **Build Win64 package** — runs on your self-hosted Windows runner; compiles the plugin via `RunUAT BuildPlugin`. **Both the source and Win64 jobs must succeed** — `continue-on-error` is off, so a failed build blocks the release.
4. **Publish GitHub Release** — attaches both zips; GitHub auto-generates release notes from merged PRs.

### Artifacts produced

| File | Purpose |
|---|---|
| `Blueprint-Exec-Flow-Viewer-{version}-source.zip` | Source-only zip for users who build from source |
| `Blueprint-Exec-Flow-Viewer-{version}-Win64.zip` | Prebuilt Win64 plugin — also what you submit to Fab manually |

Zip names use the product slug **Blueprint Exec Flow Viewer** (`PRODUCT_SLUG` in [release.yml](workflows/release.yml)). The plugin still installs under `Plugins/BPExecFlowViewer/` inside the archive.

### Submitting to Fab

Fab has no public upload API. After the release workflow completes:
1. Download `Blueprint-Exec-Flow-Viewer-{version}-Win64.zip` from the GitHub Release.
2. Log in to [fab.com](https://www.fab.com) and submit the zip through the product dashboard.

### Testing the workflow without tagging

Use **Actions → Release → Run workflow** (workflow_dispatch). The version resolves to `0.0.0-manual`; the publish-release job is skipped (tag-only), but you can verify both zips are built correctly as artifacts.

## Self-hosted runner setup

GitHub-hosted runners do not have UE installed. You need a Windows PC registered as a **self-hosted** Actions runner ([GitHub docs](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners)).

`UE_ROOT` is resolved in this order:
1. Repo **Settings → Secrets → Actions** → set `UE_ROOT` = `C:\Program Files\Epic Games\UE_5.7`
2. Windows user/system env var `UE_ROOT` on the runner PC
3. Default install path `C:\Program Files\Epic Games\UE_5.7` (no config needed if UE is there)

Keep the runner **online** when you push a version tag.

**Local package (same as CI):** run `Scripts\PackageForFab.bat`.
Optional: `set PACKAGE_OUT=C:\path\to\output\BPExecFlowViewer` before running.

**Windows path length:** CI builds to `C:\_ci\bpefv` because paths under `actions-runner\_work\…` often exceed 260 characters and UBT fails. Use a short `PACKAGE_OUT` locally if you hit the same issue.

### One runner for multiple repos

Repo-level runners only see jobs for that repo. For one `C:\actions-runner` across repos, register at the **organization** level (Org → Settings → Actions → Runners). Without an org, run separate registrations (e.g. `C:\actions-runner` and `C:\actions-runner-bpexec`) — one `run.cmd` per repo.
