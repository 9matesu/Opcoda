# Atlas index

Scope: repo root `<root>`, commit `<hash>`, date `<yyyy-mm-dd>`. Folders covered: `<list>`. Folders skipped: `<list with reason>`. Renderer: Edge `<version>` plus Mermaid `<version>`.

## Map

* `01-inventory.md`: file list grouped by role. Start here when you need a file.
* `02-architecture.md`: context and container diagrams. Start here when you need the shape.
* `03-dataflows.md`: L0 and L1 data movement. Start here when you need what moves where.
* `04-entries-exits.md`: entries, exits, and one hot path sequence. Start here when you debug I/O.

## Open questions

List what tracing could not resolve, with the file that blocks it. Example: `src/audio/engine.cpp:210: queue ownership unclear, needs runtime check`.
