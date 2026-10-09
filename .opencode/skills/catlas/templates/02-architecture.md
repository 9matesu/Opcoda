# Architecture

## Context

```mermaid
%% from diagrams/context.mmd
```

| edge | source | sink |
| ---- | ------ | ---- |
| `<label>` | `<path:line>` | `<path:line>` |

Notes: who calls the repo, what the repo calls. External systems sit outside the boundary box.

## Containers

```mermaid
%% from diagrams/containers.mmd
```

| edge | source | sink |
| ---- | ------ | ---- |
| `<label>` | `<path:line>` | `<path:line>` |

Notes: module boundaries match folders or build targets. Queue and atomic crossings are marked.
