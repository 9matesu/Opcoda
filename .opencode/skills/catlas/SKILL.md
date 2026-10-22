---
name: Catlas
description: Map a messy codebase into docs/atlas with verified Mermaid diagrams in UML gray block style. Use when asked to document, review, or trace how data enters and leaves a repo. Produces inventory, architecture, data flux, and entry exit docs with vision checked diagrams.
---

# Catlas: codebase map with readable diagrams

Catlas turns a repo you do not trust yet into a folder you can read. It inventories the files, traces where data comes in and where it goes out, and draws the result so the drawings match the code. Output lands in `docs/atlas/`. Diagrams are Mermaid so GitHub renders them. A local Edge render checks alignment before you commit.

Read `references/voice-guide.md` first. Then follow the phases in order. Do not skip the render check.

## Required skills

Load `humanizer` before you write any prose. All markdown output must pass it. Code blocks, paths, commands, and link targets stay unchanged. Prose gets rewritten until it reads like a senior engineer wrote it after reading the code. If `humanizer` is unavailable, stop. Do not ship unreviewed prose.

Load order:

1. `humanizer`
2. This skill

## Inputs and outputs

Input: repo root. Default is the current working directory.

Output root: `docs/atlas/`. Contents:

```text
docs/atlas/index.md
docs/atlas/01-inventory.md
docs/atlas/02-architecture.md
docs/atlas/03-dataflows.md
docs/atlas/04-entries-exits.md
docs/atlas/diagrams/context.mmd
docs/atlas/diagrams/containers.mmd
docs/atlas/diagrams/dataflow-l0.mmd
docs/atlas/diagrams/dataflow-l1-<name>.mmd
docs/atlas/diagrams/sequence-<name>.mmd
docs/atlas/assets/flux.json
docs/atlas/assets/inventory.csv
```

If `docs/atlas/` exists, update it in place. Keep history in git. Do not create a second folder.

## Phase 0: scope

Confirm four items with the user when they are unclear: repo root, languages present, output folder, diagram budget. Default budget is 5 diagrams plus at most 2 L1 detail diagrams. Large repos get more pages, they do not get denser diagrams. One diagram holds at most 12 nodes.

Record the scope at the top of `index.md`: commit hash, date, folders covered, folders skipped.

## Phase 1: recon

Read the root listing, the package manifests, and the likely entries. Search for `main`, `app`, `index`, `cli`, route files, plugin entry points. Note what you could not open.

Write nothing long in this phase. Collect paths with line numbers.

## Phase 2: inventory

Run `scripts/mermaid-lint.py --inventory` when available, or build the table by hand from glob output. Columns: path, language, role, entry point, notes. Role is one word: entry, logic, io, config, test, generated, vendor.

Cap the table at 60 rows. Group the rest by folder with counts. Mark generated and vendor code so readers stop trusting line counts. Save the raw list to `assets/inventory.csv`.

Common failure: listing every file and calling it documentation. If the table passes 60 rows, you missed the grouping step.

## Phase 3: entries and exits

List every inbound and every outbound. Inbound covers CLI args, HTTP handlers, file reads, env vars, queues, callbacks from the host. Outbound covers DB writes, file writes, network calls, stdout, UI updates, host callbacks.

Each row needs the exact call site as `path:line` and the data type that moves. No type, no row. Save the machine form to `assets/flux.json` with fields `id`, `kind`, `path`, `line`, `data`, `to`.

Trace from the call site, not from the README. READMEs lie in messy repos. The code at `path:line` decides.

See `references/dataflow-tracing-playbook.md` for the search patterns.

## Phase 4: flux graph

Build `assets/flux.json` before any drawing. Nodes are files or functions. Edges carry data. Every edge needs a source line and a sink line.

Prune before drawing: drop logging, drop test-only paths, fold retries into one edge. If the JSON has more than 14 edges for one diagram, split the diagram. The limit protects readability.

## Phase 5: diagrams

Draw the minimum set. Each diagram lives in its own `.mmd` file and embeds in its page.

1. `context.mmd` in `02-architecture.md`. System in the middle, users and external systems around it. Shows who calls the repo and what the repo calls.
2. `containers.mmd` in `02-architecture.md`. Top modules inside the repo. Shows compile or folder boundaries.
3. `dataflow-l0.mmd` in `03-dataflows.md`. Sources left, sinks right. One edge per data type.
4. `dataflow-l1-<name>.mmd` in `03-dataflows.md`. One per critical flux, at most 2 by default. Shows function level steps.
5. `sequence-<name>.mmd` in `04-entries-exits.md`. One hot path end to end. Shows call order with file names on the participants.

Style rules live in `references/diagram-style-guide.md`. Gray fill marks abstract or grouping nodes. White fill marks concrete files or functions. Inheritance uses `<|--`. Data movement uses `--> ` with a label. Every node label ends with the short file name. The companion table under each diagram maps each edge to `path:line`.

Start from `templates/diagram-snippets.mmd`. Do not invent a new style per diagram.

## Phase 6: visual QA

Run the checks in this order:

1. `python .opencode/skills/catlas/scripts/mermaid-lint.py`
2. `powershell -File tools/render-mermaid.ps1 -Atlas docs/atlas`
3. Read each rendered PNG with the read tool. Look for overlap, clipped labels, crossed edges, tiny fonts.
4. Fix the `.mmd` source. Never fix the PNG. The `.mmd` file is the source of truth.
5. Repeat at most 3 rounds per diagram. Then split the diagram instead.

If Edge is missing, keep the text lint gate and note the missing visual pass in `index.md`. Do not mark the atlas final.

## Phase 7: publish

Fill the templates in order: `01-inventory.md`, `02-architecture.md`, `03-dataflows.md`, `04-entries-exits.md`, then `index.md`. Each page embeds its diagrams and its trace table. Each claim cites `path:line`.

Run `humanizer` file mode over each markdown file. Keep code blocks and paths unchanged.

Launch subagents when the repo is large:

* `catlas-writer` owns the markdown pages.
* `catlas-diagrammer` owns `.mmd` files and the style guide.
* `catlas-reviewer` signs off read only.

Small repos can run inline without subagents. The phases stay the same.

## Stop conditions

Stop and ask when the entry point is unclear after recon, when `flux.json` exceeds the edge budget and you cannot split it sensibly, or when the render tool is missing and the user needs final output. Note the blocker in `index.md`.
