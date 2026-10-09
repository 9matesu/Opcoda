# Catlas

Catlas maps a messy codebase into `docs/atlas/` so a new reader can find entries, follow data, and trust the drawings. Diagrams are Mermaid in UML gray block style. GitHub renders them. A local Edge pass checks alignment.

## Install

Copy this folder to `.opencode/skills/catlas` in the target repo. Copy `humanizer` to `.opencode/skills/humanizer`. Copy `catlas-*.md` to `.opencode/agent/` (this repo uses singular `agent`, matching its existing agents). Copy `render-mermaid.ps1` to `tools/`.

Vendor the renderer once:

```powershell
# place pinned build here, then commit it
.opencode/skills/catlas/third_party/mermaid.min.js
```

Source a pinned `mermaid.min.js` from the Mermaid release that matches `index.md`. The file is not shipped here to keep the skill small.

## Run

```text
@catlas map this repo into docs/atlas
```

Phases: scope, recon, inventory, entries and exits, flux graph, diagrams, visual QA, publish. Detail lives in `SKILL.md`.

Subagents: `catlas-writer` owns pages, `catlas-diagrammer` owns `.mmd`, `catlas-reviewer` signs off read only.

## Voice

All prose passes the vendored `humanizer` skill (MIT, upstream https://github.com/blader/humanizer). Senior engineer tone: short sentences, file and line cites, no filler. Code blocks, paths, commands, and error codes stay verbatim.

## License

Skill scaffolding is yours to use. The `humanizer` copy keeps its MIT license and upstream notice.
