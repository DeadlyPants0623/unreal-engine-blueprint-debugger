# Maintainer notes

## Creating a release

```bash
git tag v1.0.0
git push origin v1.0.0
```

The [release workflow](workflows/release.yml) publishes a **source zip** on every `v*` tag. A **Win64 zip** is optional (self-hosted runner; does not block the release if it fails or the runner is offline).

## Optional: Win64 prebuilt on releases

GitHub-hosted runners do not have UE installed. To attach a Win64 zip:

1. Register your Windows PC as a **self-hosted** Actions runner ([GitHub docs](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners)).
2. Point CI at your UE install — **any one** of:
   - Repo **Settings → Secrets → Actions** → `UE_ROOT` = `C:\Program Files\Epic Games\UE_5.7`
   - Windows user/system env var `UE_ROOT` on the runner PC
   - Default install path `C:\Program Files\Epic Games\UE_5.7` (no config if UE is there)
3. Keep the runner **online** when you push a version tag if you want the Win64 asset.

**Local package (same as CI):** run `Scripts\PackageForFab.bat`. Optional: `set PACKAGE_OUT=C:\path\to\output\BPExecFlowViewer`.

### One runner for multiple repos (organization)

Repo-level runners only see jobs for that repo. For one `C:\actions-runner` across repos, register at the **organization** level (Org → Settings → Actions → Runners), then set `UE_ROOT` on each repo that uses the Win64 job.

Without an org, you can run separate registrations (e.g. `C:\actions-runner` and `C:\actions-runner-bpexec`) — one `run.cmd` per repo.

## CI

[ci.yml](workflows/ci.yml) — on push/PR, zips plugin source on GitHub-hosted Linux (smoke test; no UE compile).
