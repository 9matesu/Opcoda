# Data movement

## L0

```mermaid
%% from diagrams/dataflow-l0.mmd
```

| edge | source | sink |
| ---- | ------ | ---- |
| `<data: type>` | `<path:line>` | `<path:line>` |

Read left to right, sources to sinks. Async paths use dotted edges and name the queue.

## L1 detail: <flux name>

```mermaid
%% from diagrams/dataflow-l1-<name>.mmd
```

| step | code | data |
| ---- | ---- | ---- |
| `<n>` | `<path:line>` | `<what moves>` |

Keep one L1 per critical flux. At most 2 by default. Each step cites the line that moves the data.
