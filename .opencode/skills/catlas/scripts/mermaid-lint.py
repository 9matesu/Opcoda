"""Catlas Mermaid lint. Stdlib only. Fails on unreadable diagrams."""
import re
import sys
from pathlib import Path

NODE_PAT = re.compile(r"^\s*(?:class\s+(\w+)|(\w+)\[|(\w+)\s*-->|(\w+)\s*-->|subgraph)")
EDGE_PAT = re.compile(r"--|<\|--|\*--|-.->")
LABEL_PAT = re.compile(r"\|[^|]+\|")

MAX_NODES = 12
MAX_EDGES = 14
MAX_LABEL = 15


def lint_file(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    errors: list[str] = []
    if "direction " not in text:
        errors.append(f"{path}: missing direction line")
    nodes: set[str] = set()
    for m in re.finditer(r"class\s+(\w+)", text):
        nodes.add(m.group(1))
    for m in re.finditer(r"(\w+)\[", text):
        nodes.add(m.group(1))
    for m in re.finditer(r"participant\s+(\w+)", text):
        nodes.add(m.group(1))
    edges = EDGE_PAT.findall(text)
    if len(nodes) > MAX_NODES:
        errors.append(f"{path}: {len(nodes)} nodes, limit {MAX_NODES}, split the diagram")
    if len(edges) > MAX_EDGES:
        errors.append(f"{path}: {len(edges)} edges, limit {MAX_EDGES}, split the diagram")
    is_sequence = "sequenceDiagram" in text
    pairs: dict[frozenset[str], int] = {}
    for i, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if "-->" in s and "|" not in s and not is_sequence:
            errors.append(f"{path}:{i}: unlabeled --> edge")
        if "\u2014" in s or "\u2013" in s:
            errors.append(f"{path}:{i}: em or en dash in source, use comma or colon")
        if not is_sequence:
            for m in re.finditer(r"\|([^|]+)\|", s):
                if len(m.group(1).strip()) > MAX_LABEL:
                    errors.append(
                        f"{path}:{i}: edge label '{m.group(1).strip()}' "
                        f"over {MAX_LABEL} chars, shortens past the edge"
                    )
            for m in re.finditer(r"\[(.*?)\]", s):
                for part in m.group(1).split("<br/>"):
                    if len(part.strip()) > MAX_LABEL:
                        errors.append(
                            f"{path}:{i}: node line '{part.strip()}' "
                            f"over {MAX_LABEL} chars, spills out of the box"
                        )
            flat = re.sub(r"\|[^|]*\|", "", s)
            m = re.search(r"(\w+)\s*(?:-->|-.->|<\|--|\*--)\s*(\w+)", flat)
            if m:
                key = frozenset([m.group(1), m.group(2)])
                pairs[key] = pairs.get(key, 0) + 1
    for pair, count in pairs.items():
        if count > 2:
            names = sorted(pair)
            errors.append(
                f"{path}: {count} edges between {' and '.join(names)}, "
                "labels stack into one blob, merge or move one to L1"
            )
    if "classDiagram" in text and "classDef abstract" not in text:
        errors.append(f"{path}: classDiagram without abstract classDef, see style guide")
    return errors


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path("docs/atlas/diagrams")
    if root.is_file():
        files = [root]
    else:
        files = sorted(root.glob("*.mmd"))
    if not files:
        print(f"no .mmd files under {root}")
        return 1
    errors: list[str] = []
    for f in files:
        errors.extend(lint_file(f))
    if errors:
        print("\n".join(errors))
        return 1
    print(f"ok: {len(files)} diagrams pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
