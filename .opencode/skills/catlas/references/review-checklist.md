## Review checklist

The reviewer checks the atlas the way a senior engineer checks a new hire map of a messy repo: against the code, with line numbers.

### Trace check

Pick 3 edges at random per diagram. Grep each label in the repo. Each edge must resolve to a `path:line` pair in the companion table. A missing source fails the diagram. A label that matches nothing fails the diagram. A type that differs from the code fails the row.

### Count check

Count nodes and edges from the `.mmd` source. More than 12 nodes fails. More than 14 edges fails. Unlabeled `-->` fails. Missing `direction` line fails. Run `scripts/mermaid-lint.py` and keep its output in the review note.

### Alignment check

Read the rendered PNG at full width. Look for overlapping boxes, clipped text, edges that cross labels, fonts below readable size. Check three specific artifacts: text sticking out of a box, two edge labels stacked into one blob, a node name cut off at the box edge (`spsc_ring.n`). Name the file and the overlapping pair. Suggest the split or the subgraph move. Shorten the label per the style guide before splitting. Do not edit the diagram. The diagrammer owns fixes.

### Voice check

Run through the humanizer patterns §1 to §26 on the markdown pages. Flag contrasts of the form not X but Y, one line closers, triads, decorative bold, em dashes, inflated claims. Prose that sounds generated fails even when the facts are right. Code blocks, paths, and commands are exempt.

### Sign off format

Per diagram: file, pass or fail, evidence. Keep it short:

```text
diagrams/dataflow-l0.mmd: pass. 9 nodes, 11 edges. 3 sampled edges resolve to src/....cpp:line.
diagrams/containers.mmd: fail. Node AudioEngine overlaps Queue in render 2026-10-09. Split DSP subgraph.
```

No summary paragraph that repeats the table. The table is the report.
