# Entries and exits

## Inbound

| entry | site | data |
| ----- | ---- | ---- |
| `<cli/http/file/env/queue>` | `<path:line>` | `<type>` |

## Outbound

| exit | site | data |
| ---- | ---- | ---- |
| `<db/file/net/ui/callback/error>` | `<path:line>` | `<type>` |

Typed errors are exits. List each return site.

## Hot path

```mermaid
%% from diagrams/sequence-<name>.mmd
```

Notes: call order for the path you hit first when debugging. Participants carry file names.
