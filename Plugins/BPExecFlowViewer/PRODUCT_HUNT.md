# Product Hunt launch — Blueprint Exec Flow Viewer

Use this doc when submitting at [producthunt.com/posts/new](https://www.producthunt.com/posts/new). Product Hunt does not auto-post from this repo; copy fields below into the form.

**Suggested launch day:** Tuesday, Wednesday, or Thursday, scheduled for **12:01 AM Pacific** so the listing gets a full 24-hour window.

---

## Form fields (copy-paste)

### Name

```
Blueprint Exec Flow Viewer
```

### Tagline (max 60 characters)

```
Visualize Blueprint exec flow from any node in UE 5.7
```

*(59 characters)*

**Alternates if you want a different angle:**

| Tagline | Chars |
|---------|-------|
| `Right-click a Blueprint node—see full exec flow` | 48 |
| `Trace Blueprint execution upstream and downstream` | 50 |
| `Debug Blueprint logic paths without running PIE` | 48 |

### Link

Primary (pick one):

- **GitHub repo / releases:** `https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger`
- **Direct download:** `https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases`

Optional second link in comments: Fab listing URL once live.

### Description

```
Blueprint Exec Flow Viewer is an Unreal Engine 5.7 editor plugin that maps execution flow around any Blueprint node—callers on the left, your node in the centre, callees on the right.

Stop scrolling through spaghetti graphs. Right-click a node in the Blueprint Editor, choose View Exec Flow, and get a read-only dockable graph with route labels (Branch: True, IsValid: Valid, Exec, and more).

Built for Blueprint-heavy projects:
• Cross-Blueprint tracing via CallFunction when assets resolve in the Asset Registry
• Click any row to jump back to the source node in the Blueprint Editor
• Re-root (→) to re-trace from a different step as the new centre
• Adjustable backward (0–32) and forward (0–32) depth—defaults 2 / 4
• Zoom-to-fit after every rebuild

Editor-only and static (graph topology, not runtime PIE trace)—fast clarity while you design, not a replacement for breakpoints.

Free on GitHub (MIT). Also on Fab for UE marketplace installs. Requires UE 5.7 and a C++ project (or one-time Add C++ Class) for source builds.
```

### Topics (select on Product Hunt)

Suggested tags — pick what Product Hunt offers that closest match:

- Developer Tools
- Open Source
- Productivity
- Gaming (if available)
- Design Tools (secondary)

### Pricing

```
Free
```

(or **Open source** / **Free download** depending on PH UI)

### Makers

Add yourself (and any co-makers). Link GitHub / X / personal site in maker profiles before launch day.

---

## First comment (post immediately after publish)

Product Hunt ranks engagement early. Post this as the **first comment** from the maker account:

```
Hey Product Hunt 👋

I'm CJ, maker of Blueprint Exec Flow Viewer.

I built this because tracing "what runs before and after this node?" in Unreal Blueprints usually means manual graph hopping—especially when CallFunction jumps into another Blueprint asset.

This plugin adds one workflow: right-click → View Exec Flow. You get a column layout (callers ← root → callees), labeled branches, and click-to-jump rows so you can stay in flow while debugging design-time logic.

A few honest notes:
• It's editor-only—static exec topology, not a Play-In-Editor runtime tracer
• Cross-Blueprint hops depend on Asset Registry / compile state
• UE 5.7 + C++ project (or add any C++ class once) for source installs

I'd love feedback from Blueprint-heavy teams:
1. What node types do you wish were first-class in the context menu?
2. Would runtime trace (PIE) be more valuable than this static map?

GitHub releases (MIT): https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases

Happy to answer questions all day—thanks for checking it out 🙏
```

---

## Gallery & thumbnail (you must supply images)

Product Hunt expects visual proof. Capture from the UE 5.7 editor:

| Asset | Size | Content |
|-------|------|---------|
| **Thumbnail** | 240×240 (square) | Plugin icon or cropped graph with gold centre column |
| **Gallery 1** | 1270×760 recommended | Full Exec Flow panel: graph + depth spinners + Rebuild |
| **Gallery 2** | 1270×760 | Right-click context menu showing **View Exec Flow** |
| **Gallery 3** | 1270×760 | Cross-Blueprint trace (multiple Blueprint cards / columns) |
| **Gallery 4** | 1270×760 | Click-to-jump: row hover + Blueprint Editor focused on target node |
| **Optional video** | 30–90 sec | Screen recording: right-click → graph → click row → jump |

**Capture tips**

- Use a clean demo Blueprint (readable names, Branch + IsValid visible).
- Dark editor theme reads better on Product Hunt.
- Hide unrelated nomad tabs; frame only Blueprint Editor + Exec Flow panel.
- Add short text overlays in Figma/Canva only if they stay readable at mobile width.

No screenshots ship in the repo yet—record before launch.

---

## Social posts (launch day)

### X / Twitter

```
Launching on @ProductHunt today: Blueprint Exec Flow Viewer 🎮

Right-click any Blueprint node in UE 5.7 → see upstream/downstream exec flow in one graph (cross-BP, branch labels, click-to-jump).

Free & open source (MIT).

Would mean a lot if you tried it + left a comment 🙏
👇 [Product Hunt URL]
```

### LinkedIn (shorter)

```
We just launched Blueprint Exec Flow Viewer on Product Hunt.

It's an Unreal Engine 5.7 editor plugin that visualizes Blueprint execution flow from any node—callers, callees, and branch routes in one dockable graph.

If you work with Blueprints, I'd appreciate your feedback: [Product Hunt URL]
```

### Discord / forums (Unreal Slack, Reddit r/unrealengine)

```
[Launch] Blueprint Exec Flow Viewer — PH link inside

Editor plugin for UE 5.7: right-click a node → View Exec Flow → column graph of exec paths, including cross-Blueprint calls. MIT, free on GitHub.

Not runtime PIE trace—static topology for design-time clarity.

Product Hunt: [URL]
GitHub: https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/releases
```

---

## Pre-launch checklist

- [ ] Product Hunt account in good standing; maker profile filled out
- [ ] Thumbnail + 3–4 gallery images (or short video) captured
- [ ] GitHub release **v1.0.0** (or latest) published with install zip
- [ ] README install steps verified on a clean UE 5.7 project
- [ ] Fab listing live (optional)—link in first comment if so
- [ ] Hunter recruited (optional)—someone with PH followers to post for you
- [ ] 5–10 friends/colleagues briefed to upvote + **comment** in first 2 hours (comments weigh more than drive-by upvotes)
- [ ] Post scheduled 12:01 AM PT; maker awake ~6 AM PT for EU morning wave
- [ ] Pin PH link on GitHub README for the launch week (optional)

---

## After launch

- Reply to every PH comment within a few hours
- Add top FAQ answers to GitHub Issues or README if the same questions repeat
- Thank supporters on X with a screenshot of the graph (not vote counts—PH discourages vote begging)
- Archive PH badge + final rank in `CHANGELOG.md` if you want a public trail

---

## Quick reference

| Item | Value |
|------|--------|
| Product | Blueprint Exec Flow Viewer |
| Engine | Unreal Engine 5.7 |
| License | MIT |
| Repo | https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger |
| Support | https://github.com/DeadlyPants0623/unreal-engine-blueprint-debugger/issues |
