## Diagram style guide

Mermaid renders on GitHub without extra tooling. That is why Catlas uses it. The default theme looks rough, so this guide pins the subset that stays readable.

### File layout

One diagram per file in `docs/atlas/diagrams/`. Name matches usage: `context.mmd`, `containers.mmd`, `dataflow-l0.mmd`, `dataflow-l1-<name>.mmd`, `sequence-<name>.mmd`. The markdown page embeds the file with a mermaid fence and keeps the same source in the `.mmd` file. Edit the `.mmd` file. Copy into the page on publish.

### Header

Every `classDiagram` and `flowchart` starts with a direction line. Use `direction LR` for data movement, left to right from sources to sinks. Use `direction TB` for inheritance, parents above children. One direction per diagram. Mixed directions cause the angled edges that make Mermaid hard to read.

### UML gray blocks

The reference look is gray group boxes with white leaves. Recreate it with two classDefs:

```mermaid
classDiagram
  classDef abstract fill:#9AA0A6,stroke:#5F6368,color:#fff,fontStyle:italic
  classDef concrete fill:#ffffff,stroke:#8B1A1A,stroke-width:1px,color:#202124
```

Apply `:::abstract` to grouping nodes, base classes, and anything marked abstract in code. Apply `:::concrete` to files and functions that hold code. Keep the palette fixed. Readers learn it once and reuse it across pages.

Font: `Consolas, monospace` for node text when the renderer allows theme variables. Straight quotes only.

### Edge vocabulary

Keep four edge types:

* `<|--` for inheritance. Vertical only.
* `*--` for composition. Use sparingly.
* `-->` with a label for data movement. The label names the data and the type, for example `pe_bytes: vector<uint8_t>`.
* `-.->` for async, queued, or callback paths. Add `queue` or `callback` to the label.

No unlabeled arrows. An edge without a label failed tracing.

### Readability limits

* At most 12 nodes per diagram.
* At most 14 edges per diagram.
* Node labels end with the short file name, for example `Parser [pe_parser.cpp]`.
* Subgraph per layer or trust boundary. Name the subgraph after the folder or the process boundary.
* Break long labels with `<br/>`. Avoid horizontal scroll.

When a diagram exceeds the limits, split by flux or by layer. Denser diagrams do not carry more information. They carry less, because nobody reads them.

### Label fit

Mermaid sizes boxes from the label text, and long labels overflow the box or the edge. Keep every label line short:

* Node label lines: at most 15 characters per `<br/>` line. Strip the extension and the folder when the file name is longer (`byte_to_sample.cpp` becomes `byte_to_sample`). The full path lives in the companion table, so nothing is lost.
* Edge labels: name only, at most 15 characters (`pe_bytes`, not `pe_bytes: vector`). The type lives in the companion table and in `flux.json`.
* One edge per node pair per direction in context diagrams. Two data types sharing a path share one edge (`pe_bytes, table`), with the split drawn in L1. Three edges between the same pair always stack their labels into one unreadable blob.
* Sequence diagrams are exempt from the length cap. Their arrows run long, so full file names fit. The reviewer still eyeballs them.

### Companion table

Under each diagram, keep a table with columns `edge`, `source`, `sink`. Source and sink are `path:line`. The table is the proof. A node with no table row gets removed or traced. Reviewers check this table first.

### Theme variables for flowcharts

```text
themeVariables:
  primaryColor: "#ffffff"
  primaryBorderColor: "#8B1A1A"
  lineColor: "#5F6368"
  fontFamily: "Consolas, monospace"
```

No shadows. No rounded corners where the renderer allows square. No per diagram colors beyond the two classDefs.

### Known limits

Mermaid draws splines. It will not produce the right angle elbows PlantUML draws. Inheritance triangles vary in size. Accept the 80 percent match on GitHub. The committed SVG from the Edge render pass is the stable visual record. Note renderer version in `index.md` when alignment looks off after an upgrade.
