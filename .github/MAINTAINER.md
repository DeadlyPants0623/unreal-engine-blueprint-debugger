# Maintainer notes

## Creating a release

Before tagging, verify the plugin compiles locally:

```bat
Scripts\PackageForFab.bat
```

(`PackageForFab.bat` runs `RunUAT BuildPlugin` for validation only — do **not** upload the `Packaged\` output to Fab.)

Then tag and push:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The [release workflow](workflows/release.yml) publishes one zip on every `v*` tag:

- **File:** `Blueprint-Exec-Flow-Viewer-{version}.zip`
- **Contents:** plugin source only (no `Binaries`, `Intermediate`, `Build`, or `Saved`)
- **GitHub:** created as a **pre-release** — edit the release and uncheck pre-release when ready for “Latest”

Use the same zip for:

- GitHub Releases (user install: extract into `YourProject/Plugins/BPExecFlowViewer/`)
- Fab “Project File Link” (direct asset URL from the release)

Example Fab/download URL:

`https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases/download/v1.0.0/Blueprint-Exec-Flow-Viewer-1.0.0.zip`

### Re-publish a tag (e.g. fix a failed release)

```bash
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0
git tag v1.0.0
git push origin v1.0.0
```

Delete the old GitHub Release for that tag first if it exists.

## CI

[ci.yml](workflows/ci.yml) — on push/PR, zips plugin source on GitHub-hosted Linux and asserts the archive excludes build artifacts.
