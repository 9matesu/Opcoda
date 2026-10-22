#!/usr/bin/env python3
"""Catlas Mermaid -> TikZ.

Transpila o subconjunto Mermaid que o Catlas usa (flowchart, classDiagram,
sequenceDiagram) para TikZ. O objetivo e' ter o mesmo diagrama em duas formas a
partir de uma so fonte: Mermaid para o GitHub renderizar, TikZ para o documento
LaTeX. O ficheiro .mmd e' a fonte da verdade; este script nunca escreve nele.

O subconjunto e' o mesmo que o `mermaid-lint.py` da skill do Catlas aceita:
12 nos, 14 arestas, uma direcao por diagrama, rotulos com no maximo 15
caracteres, e as duas classDefs cinza do guia de estilo.

Uso:
    python tools/mmd2tikz.py --in docs/atlas/diagrams --out docs/atlas/figuras
    python tools/mmd2tikz.py --in docs/atlas/diagrams/context.mmd --out -   (stdout)

Somente biblioteca padrao, como o resto das ferramentas de documentacao do
repositorio. Roda em Windows, Linux e macOS.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

# Paleta: amarelo e preto. As caixas sao amarelas com contorno preto, o
# texto e' preto, e os grupos recebem um amarelo mais escuro para se
# distinguirem das caixas de ficheiro.
YELLOW = "FFD400"
AMBER = "FFB000"
BLACK = "000000"

# Largura de um caractere em \small com newtxtext, em cm. Medido em LaTeX: o
# pior caso observado ("Arquivo PE") da 0,166 cm/char. O valor aqui e' um pouco
# maior para sobrar margem, e o texto mais largo e' o que decide a largura da
# caixa. Um texto que nao cabe na caixa e' partido em duas linhas, e o desenho
# perde a forma sem erro nenhum.
CHAR_CM = 0.18
# Margem da caixa: o `inner sep` do TikZ dos dois lados. A largura da caixa e'
# a do texto mais esta margem, e e' por isso que duas caixas da mesma coluna
# com a mesma largura de texto sao iguais as vistas.
PAD_CM = 0.5
BOX_PAD_CM = 0.36
LAYER_GAP_CM = 1.9
ROW_GAP_CM = 1.55
# Largura maxima de uma fila. O atlas e' uma pagina deitada, com 25,7 cm de
# texto, e a cadeia de camadas parte-se em filas antes de passar la.
MAX_ROW_CM = 24.6
ROW_BREAK_CM = 2.4
BAND_GAP_CM = 1.3
TB_LAYER_CM = 2.45
SEQ_GAP_CM = 0.7
SEQ_ROW_CM = 1.3

NODE_RE = re.compile(
    r"(?P<id>[A-Za-z_][A-Za-z0-9_]*)\s*\[(?P<label>[^\]]*)\]"
    r"(?:\s*:::\s*(?P<cls>[A-Za-z0-9_]+))?"
)
EDGE_RE = re.compile(
    r"^\s*(?P<src>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<op>-->|-\.->|-\.-|==>|--x|-\))\s*"
    r"(?:\|(?P<label>[^|]*)\|\s*)?"
    r"(?P<dst>[A-Za-z_][A-Za-z0-9_]*)\s*$"
)
SUBGRAPH_RE = re.compile(
    r"^\s*subgraph\s+(?P<id>[A-Za-z_][A-Za-z0-9_]*)\s*\[(?P<label>[^\]]*)\]\s*$"
)
CLASSDEF_RE = re.compile(r"^\s*classDef\s+")
CLASS_LINE_RE = re.compile(
    r"^\s*class\s+(?P<id>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?:\s*:::\s*(?P<cls>[A-Za-z0-9_]+))?\s*$"
)
CLASS_REL_RE = re.compile(
    r"^\s*(?P<src>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<op><\|--|\*--|-->|-\.->|..>|o--)\s*"
    r"(?:\|(?P<label>[^|]*)\|\s*)?"
    r"(?P<dst>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?::\s*(?P<text>.*?))?\s*$"
)
PARTICIPANT_RE = re.compile(
    r"^\s*participant\s+(?P<id>[A-Za-z_][A-Za-z0-9_]*)(?:\s+as\s+(?P<label>.+?))?\s*$"
)
MESSAGE_RE = re.compile(
    r"^\s*(?P<src>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<op>-{1,2}>{1,2}|--x|-\))\s*"
    r"(?P<dst>[A-Za-z_][A-Za-z0-9_]*)\s*:\s*(?P<text>.*?)\s*$"
)


# --------------------------------------------------------------------------
# Modelos
# --------------------------------------------------------------------------


@dataclass
class Node:
    id: str
    label: str
    cls: str = "concrete"
    members: list[str] = field(default_factory=list)
    layer: int = 0
    row: int = 0
    band: tuple = ()
    # Largura do texto e largura total da caixa. Todas as caixas de uma
    # coluna ficam com a mesma largura, para a grelha fechar.
    text_width: float = 0.0
    x: float = 0.0
    y: float = 0.0
    width: float = 0.0
    height: float = 0.0

    @property
    def lines(self) -> list[str]:
        return [part for part in self.label.split("<br/>") if part != ""] or [""]


@dataclass
class Edge:
    src: str
    dst: str
    op: str = "-->"
    label: str = ""


@dataclass
class Group:
    id: str
    label: str
    members: list[str] = field(default_factory=list)
    direction: str = ""


@dataclass
class Message:
    src: str
    dst: str
    op: str = "->>"
    text: str = ""


@dataclass
class Diagram:
    kind: str = "flowchart"
    direction: str = "LR"
    nodes: list[Node] = field(default_factory=list)
    edges: list[Edge] = field(default_factory=list)
    groups: list[Group] = field(default_factory=list)
    participants: list[Node] = field(default_factory=list)
    messages: list[Message] = field(default_factory=list)
    # Onde acaba a faixa vertical de cada grupo, para as arestas que saltam
    # camadas passarem por baixo da faixa e nao do desenho todo.
    band_bottom: dict = field(default_factory=dict)

    def node(self, node_id: str) -> Node | None:
        for candidate in self.nodes:
            if candidate.id == node_id:
                return candidate
        return None


# --------------------------------------------------------------------------
# Texto
# --------------------------------------------------------------------------


def latex_escape(text: str) -> str:
    """Escapa o texto para o LaTeX, preservando acentos UTF-8."""
    replacements = {
        "\\": r"\textbackslash{}",
        "&": r"\&",
        "%": r"\%",
        "$": r"\$",
        "#": r"\#",
        "_": r"\_",
        "{": r"\{",
        "}": r"\}",
        "~": r"\textasciitilde{}",
        "^": r"\textasciicircum{}",
    }
    out = []
    for char in text:
        out.append(replacements.get(char, char))
    return "".join(out)


def label_to_latex(label: str) -> str:
    """Converte um rotulo Mermaid em texto TikZ com quebras de linha."""
    parts = [latex_escape(part.strip()) for part in label.split("<br/>")]
    parts = [part for part in parts if part != ""]
    return r"\\".join(parts) if parts else ""


def estimate_text_width(node: Node) -> float:
    """Largura do texto de um rotulo, em cm.

    As medidas vem de \\small sem serifa, medidas em LaTeX: uma minuscula da
    0,165 cm, uma maiuscula 0,23 cm e um espaco 0,085 cm. Os 0,10 cm de
    margem cobrem a diferenca entre a media e o pior caso, e um rotulo que
    nao cabe na caixa parte-se em duas linhas sem erro nenhum.
    """
    longest = max(node.lines, key=len, default="")
    width = 0.0
    for char in longest:
        if char == " ":
            width += 0.085
        elif char.isupper():
            width += 0.23
        elif char.isalnum() or char == "_":
            width += 0.165
        else:
            width += 0.12
    return width + 0.10


def estimate_width(node: Node) -> float:
    """Largura total da caixa, em cm."""
    return estimate_text_width(node) + BOX_PAD_CM


# --------------------------------------------------------------------------
# Leitura
# --------------------------------------------------------------------------


def strip_comment(line: str) -> str:
    """Remove o comentario Mermaid. Um %% dentro de um rotulo e' texto."""
    result = []
    in_bracket = False
    index = 0
    while index < len(line):
        char = line[index]
        if char == "[":
            in_bracket = True
        elif char == "]":
            in_bracket = False
        if not in_bracket and line.startswith("%%", index):
            break
        result.append(char)
        index += 1
    return "".join(result).strip()


def split_statements(line: str) -> list[str]:
    """Separa os comandos de uma linha. Os rotulos ficam intactos."""
    statements = []
    current = []
    depth = 0
    for char in line:
        if char in "[(":
            depth += 1
        elif char in "])":
            depth -= 1
        if char == ";" and depth <= 0:
            statements.append("".join(current).strip())
            current = []
            continue
        current.append(char)
    tail = "".join(current).strip()
    if tail:
        statements.append(tail)
    return statements


def parse_flowchart(lines: list[str]) -> Diagram:
    diagram = Diagram(kind="flowchart")
    group_stack: list[Group] = []

    for raw in lines:
        line = strip_comment(raw)
        if not line:
            continue
        if line.startswith("flowchart"):
            diagram.direction = line.split()[1].upper() if len(line.split()) > 1 else "LR"
            continue
        if line.startswith("direction "):
            # Uma direcao dentro de uma subgrafos e' da subgrafos. Sem esta
            # distincao, a direccao interna do grupo manda na direccao do
            # desenho inteiro e o diagrama sai de pe.
            if group_stack:
                group_stack[-1].direction = line.split()[1].upper()
            else:
                diagram.direction = line.split()[1].upper()
            continue
        if CLASSDEF_RE.match(line):
            continue

        subgraph = SUBGRAPH_RE.match(line)
        if subgraph:
            group = Group(subgraph.group("id"), subgraph.group("label"))
            diagram.groups.append(group)
            group_stack.append(group)
            continue
        if line == "end":
            if group_stack:
                group_stack.pop()
            continue

        for statement in split_statements(line):
            if not statement:
                continue
            for match in NODE_RE.finditer(statement):
                node_id = match.group("id")
                existing = diagram.node(node_id)
                if existing is None:
                    node = Node(node_id, match.group("label"), match.group("cls") or "concrete")
                    diagram.nodes.append(node)
                    if group_stack:
                        group_stack[-1].members.append(node_id)
                elif match.group("cls"):
                    existing.cls = match.group("cls")

            edge = EDGE_RE.match(statement)
            if edge:
                src = edge.group("src")
                dst = edge.group("dst")
                for node_id in (src, dst):
                    if diagram.node(node_id) is None:
                        diagram.nodes.append(Node(node_id, node_id))
                diagram.edges.append(
                    Edge(src, dst, edge.group("op"), (edge.group("label") or "").strip())
                )
    return diagram


def parse_classdiagram(lines: list[str]) -> Diagram:
    diagram = Diagram(kind="classDiagram")

    for raw in lines:
        line = strip_comment(raw)
        if not line:
            continue
        if line.startswith("classDiagram"):
            continue
        if line.startswith("direction "):
            diagram.direction = line.split()[1].upper()
            continue
        if CLASSDEF_RE.match(line):
            continue

        declaration = CLASS_LINE_RE.match(line)
        if declaration:
            node_id = declaration.group("id")
            if diagram.node(node_id) is None:
                diagram.nodes.append(
                    Node(node_id, node_id, declaration.group("cls") or "concrete")
                )
            else:
                diagram.node(node_id).cls = declaration.group("cls") or "concrete"
            continue

        relation = CLASS_REL_RE.match(line)
        if relation:
            src = relation.group("src")
            dst = relation.group("dst")
            for node_id in (src, dst):
                if diagram.node(node_id) is None:
                    diagram.nodes.append(Node(node_id, node_id))
            diagram.edges.append(
                Edge(
                    src,
                    dst,
                    relation.group("op"),
                    (relation.group("label") or relation.group("text") or "").strip(),
                )
            )
    return diagram


def parse_sequence(lines: list[str]) -> Diagram:
    diagram = Diagram(kind="sequenceDiagram")

    for raw in lines:
        line = strip_comment(raw)
        if not line:
            continue
        if line.startswith("sequenceDiagram"):
            continue

        participant = PARTICIPANT_RE.match(line)
        if participant:
            node_id = participant.group("id")
            label = participant.group("label") or node_id
            diagram.participants.append(Node(node_id, label))
            continue

        message = MESSAGE_RE.match(line)
        if message:
            src = message.group("src")
            dst = message.group("dst")
            known = {node.id for node in diagram.participants}
            for node_id in (src, dst):
                if node_id not in known:
                    diagram.participants.append(Node(node_id, node_id))
            diagram.messages.append(
                Message(src, dst, message.group("op"), message.group("text"))
            )
    return diagram


def parse(text: str) -> Diagram:
    lines = text.splitlines()
    body = "\n".join(strip_comment(line) for line in lines)
    if "sequenceDiagram" in body:
        return parse_sequence(lines)
    if "classDiagram" in body:
        return parse_classdiagram(lines)
    return parse_flowchart(lines)


# --------------------------------------------------------------------------
# Layout
# --------------------------------------------------------------------------


def assign_layers(diagram: Diagram) -> dict[str, int]:
    """Camada de cada no: caminho mais longo a partir das origens.

    Os ciclos quebram-se antes. Um grafo com ciclo, como o host que recebe
    audio e manda MIDI, faz a conta crescer sem limite e as camadas deixam
    de significar alguma coisa. A quebra e' a de uma ordenacao topologica:
    a aresta que fecha o ciclo deixa de contar para a profundidade.
    """
    known = {node.id for node in diagram.nodes}
    forward: list[Edge] = []
    visited: set[str] = set()
    on_stack: set[str] = set()
    order: list[str] = []

    def visit(node_id: str) -> None:
        visited.add(node_id)
        on_stack.add(node_id)
        for edge in diagram.edges:
            if edge.src != node_id or edge.dst not in known:
                continue
            if edge.dst in on_stack:
                continue  # aresta de retorno, fecha o ciclo
            if edge.dst not in visited:
                visit(edge.dst)
            if edge.dst not in on_stack:
                forward.append(edge)
        on_stack.discard(node_id)

    for node in diagram.nodes:
        if node.id not in visited:
            visit(node.id)

    layer = {node.id: 0 for node in diagram.nodes}
    for _ in range(len(diagram.nodes) + 1):
        changed = False
        for edge in forward:
            if edge.src not in layer or edge.dst not in layer:
                continue
            if edge.src == edge.dst:
                continue
            candidate = layer[edge.src] + 1
            if candidate > layer[edge.dst]:
                layer[edge.dst] = candidate
                changed = True
        if not changed:
            break
    return layer


def order_layers(diagram: Diagram, layer: dict[str, int]) -> dict[str, int]:
    """Ordena os nos dentro de cada camada, para reduzir cruzamentos."""
    layers: dict[int, list[Node]] = {}
    for node in diagram.nodes:
        layers.setdefault(layer[node.id], []).append(node)

    position = {}
    for members in layers.values():
        for index, node in enumerate(members):
            position[node.id] = index

    predecessors: dict[str, list[str]] = {node.id: [] for node in diagram.nodes}
    for edge in diagram.edges:
        if edge.src in position and edge.dst in position and edge.src != edge.dst:
            predecessors[edge.dst].append(edge.src)

    for _ in range(3):
        for depth in sorted(layers):
            members = layers[depth]
            if depth == 0:
                continue
            def barycenter(node: Node) -> float:
                parents = predecessors.get(node.id, [])
                if not parents:
                    return float(position.get(node.id, 0))
                return sum(position.get(parent, 0) for parent in parents) / len(parents)

            members.sort(key=barycenter)
            for index, node in enumerate(members):
                position[node.id] = index

    return position


def group_blocks(layers: dict[int, list[Node]], groups: list[Group]) -> list[list[int]]:
    """Camadas que nao podem ser partidas entre filas.

    Uma subgrafos que atravessa varias camadas tem de ficar inteira numa
    fila. Se a cadeia se partir a meio de um grupo, a caixa do grupo fica
    a abraçar as duas filas e o desenho fica ilegivel.
    """
    depth_of: dict[str, int] = {}
    for depth, members in layers.items():
        for node in members:
            depth_of[node.id] = depth

    parent: dict[int, int] = {depth: depth for depth in layers}

    def find(depth: int) -> int:
        while parent[depth] != depth:
            parent[depth] = parent[parent[depth]]
            depth = parent[depth]
        return depth

    def union(a: int, b: int) -> None:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[max(ra, rb)] = min(ra, rb)

    for group in groups:
        depths = [depth_of[name] for name in group.members if name in depth_of]
        if not depths:
            continue
        lowest, highest = min(depths), max(depths)
        for depth in range(lowest, highest + 1):
            if depth in parent:
                union(lowest, depth)

    blocks: dict[int, list[int]] = {}
    for depth in sorted(layers):
        blocks.setdefault(find(depth), []).append(depth)

    ordered = [blocks[key] for key in sorted(blocks)]
    ordered.sort(key=lambda block: block[0])
    return ordered


def band_levels(bands: dict[tuple, list[Node]]) -> dict[tuple, int]:
    """Nivel vertical de cada faixa.

    Duas faixas cujas colunas se sobrepoem nao podem estar no mesmo nivel,
    senao as caixas de uma ficariam por cima das caixas da outra. Duas faixas
    em colunas distintas podem, e e' isso que poe o sistema no meio: as
    fontes a esquerda, os destinos a direita.
    """
    intervals: dict[tuple, tuple[int, int]] = {}
    for key, members in bands.items():
        layers = [node.layer for node in members]
        intervals[key] = (min(layers), max(layers))

    order = sorted(bands, key=lambda key: (intervals[key][0], intervals[key][1]))
    levels: dict[tuple, int] = {}
    placed: list[list[tuple[int, int]]] = []
    for key in order:
        low, high = intervals[key]
        level = 0
        while level < len(placed):
            clash = any(
                not (high < other_low or low > other_high)
                for other_low, other_high in placed[level]
            )
            if not clash:
                break
            level += 1
        if level == len(placed):
            placed.append([])
        placed[level].append(intervals[key])
        levels[key] = level
    return levels


def layout_flowchart(diagram: Diagram) -> None:
    if not diagram.nodes:
        return
    for node in diagram.nodes:
        node.text_width = estimate_text_width(node)
        node.width = node.text_width + BOX_PAD_CM
        node.height = 0.95 if len(node.lines) > 1 else 0.72

    layer = assign_layers(diagram)
    for node in diagram.nodes:
        node.layer = layer.get(node.id, 0)
    position = order_layers(diagram, layer)

    layers: dict[int, list[Node]] = {}
    for node in diagram.nodes:
        layers.setdefault(layer[node.id], []).append(node)

    # Todas as caixas de uma linha ficam com a mesma largura. E' o que fecha
    # a grelha: sem isto, duas caixas da mesma linha com nomes de
    # comprimento diferente ficam com bordas desalinhadads.
    for depth, members in layers.items():
        text_width = max(node.text_width for node in members)
        for node in members:
            node.text_width = text_width
            node.width = text_width + BOX_PAD_CM

    if diagram.direction == "TB":
        for depth in sorted(layers):
            members = layers[depth]
            gap = LAYER_GAP_CM
            total = sum(node.width for node in members) + gap * (len(members) - 1)
            cursor = -total / 2.0
            for node in members:
                node.x = cursor + node.width / 2.0
                node.y = -depth * TB_LAYER_CM
                cursor += node.width + gap
        return

    # Direccao LR: camadas horizontais, nos empilhados dentro de cada uma.
    # A cadeia parte-se em filas quando passa de MAX_ROW_CM. A partilha e'
    # feita por blocos, e nao por camada, para que um grupo nunca fique
    # partido entre duas filas.
    layer_width = {depth: max(node.width for node in layers[depth]) for depth in layers}
    blocks = group_blocks(layers, diagram.groups)

    rows: list[list[list[int]]] = []
    current: list[list[int]] = []
    current_width = 0.0
    for block in blocks:
        block_width = sum(layer_width[depth] for depth in block)
        block_width += LAYER_GAP_CM * (len(block) - 1)
        needed = block_width if current_width == 0.0 else block_width + LAYER_GAP_CM
        if current_width > 0.0 and current_width + needed > MAX_ROW_CM:
            rows.append(current)
            current = []
            current_width = 0.0
            needed = block_width
        current.append(block)
        current_width += needed
    if current:
        rows.append(current)

    # A posicao horizontal de cada camada e' a mesma em todas as filas, para
    # que as arestas que atravessam filas fiquem verticais.
    column_x: dict[int, float] = {}
    cursor = 0.0
    for depth in sorted(layers):
        column_x[depth] = cursor + layer_width[depth] / 2.0
        cursor += layer_width[depth] + LAYER_GAP_CM

    band_bottom_all: dict[tuple, float] = {}
    for row_index, row in enumerate(rows):
        drop = row_index * (ROW_BREAK_CM + ROW_GAP_CM)

        # Cada grupo e' uma faixa vertical dentro da fila. Sem isto, os
        # grupos que partilham camadas teriam as caixas sobrepostas, porque
        # a caixa de um grupo envolve todos os seus membros.
        band_of: dict[str, tuple] = {}
        for group in diagram.groups:
            for name in group.members:
                band_of[name] = ("g", group.id)
        for node in diagram.nodes:
            if node.id not in band_of:
                band_of[node.id] = ("l", node.layer)

        members_in_row = [
            node for node in diagram.nodes
            if node.layer in {depth for block in row for depth in block}
        ]
        bands: dict[tuple, list[Node]] = {}
        for node in members_in_row:
            bands.setdefault(band_of[node.id], []).append(node)

        def band_order(key: tuple) -> tuple:
            members = bands[key]
            lowest = min(node.layer for node in members)
            # Os grupos entram antes dos nos soltos com a mesma camada, e
            # pela ordem em que foram declarados.
            group_order = 0
            if key[0] == "g":
                group_order = [g.id for g in diagram.groups].index(key[1])
            return (lowest, 0 if key[0] == "g" else 1, group_order)

        band_keys = sorted(bands, key=band_order)
        levels = band_levels(bands)

        # As faixas de um nivel partilham a mesma altura, e cada uma comeca
        # no topo do nivel. As faixas do nivel seguinte ficam por baixo.
        band_rows: dict[int, int] = {}
        for key in band_keys:
            members = bands[key]
            per_layer: dict[int, int] = {}
            for node in members:
                per_layer[node.layer] = per_layer.get(node.layer, 0) + 1
            band_rows[levels[key]] = max(
                band_rows.get(levels[key], 0), max(per_layer.values()))

        level_top: dict[int, float] = {}
        cursor = 0.0
        for level in sorted(band_rows):
            level_top[level] = cursor
            cursor -= band_rows[level] * ROW_GAP_CM + BAND_GAP_CM

        band_bottom: dict[tuple, float] = {}
        for key in band_keys:
            members = bands[key]
            per_layer: dict[int, int] = {}
            for node in members:
                per_layer[node.layer] = per_layer.get(node.layer, 0) + 1
            band_height = max(per_layer.values()) * ROW_GAP_CM
            top = level_top[levels[key]] - drop
            index_in_layer: dict[int, int] = {}
            for node in sorted(members, key=lambda n: (n.layer, n.id)):
                slot = index_in_layer.get(node.layer, 0)
                index_in_layer[node.layer] = slot + 1
                node.y = top - slot * ROW_GAP_CM
                node.band = key
            band_bottom[key] = top - band_height
            band_bottom_all[key] = top - band_height

        # Agora as colunas: a largura de uma camada e' a do no mais largo
        # que nela esta, seja de que faixa vier.
        for block in row:
            for depth in block:
                members = layers[depth]
                for node in members:
                    node.x = column_x[depth]
                    node.row = row_index
    diagram.band_bottom = band_bottom_all


def layout_classdiagram(diagram: Diagram) -> None:
    """Heranca desenha os pais acima dos filhos."""
    parents: dict[str, list[str]] = {node.id: [] for node in diagram.nodes}
    children: dict[str, list[str]] = {node.id: [] for node in diagram.nodes}
    for edge in diagram.edges:
        if edge.op == "<|--":
            parents[edge.dst].append(edge.src)
            children[edge.src].append(edge.dst)

    depth = {node.id: 0 for node in diagram.nodes}

    def compute(node_id: str, seen: frozenset[str]) -> int:
        if node_id in seen:
            return 0
        if not parents.get(node_id):
            return 0
        value = 1 + max(compute(parent, seen | {node_id}) for parent in parents[node_id])
        depth[node_id] = value
        return value

    for node in diagram.nodes:
        compute(node.id, frozenset())

    rows: dict[int, list[Node]] = {}
    for node in diagram.nodes:
        rows.setdefault(depth[node.id], []).append(node)

    y = 0.0
    for level in sorted(rows):
        members = rows[level]
        # Uma classe larga parte a linha em varias, senao o desenho sai com
        # o dobro da largura da pagina.
        line: list[Node] = []
        lines: list[list[Node]] = []
        x = 0.0
        for node in members:
            node.text_width = max(1.6, len(node.label) * CHAR_CM)
            node.width = node.text_width + BOX_PAD_CM
            node.height = 0.85
            if line and x + node.width > MAX_ROW_CM:
                lines.append(line)
                line = []
                x = 0.0
            node.x = x + node.width / 2.0
            x += node.width + 0.8
            line.append(node)
        if line:
            lines.append(line)

        for sub, members_of_line in enumerate(lines):
            # Todas as classes de uma linha ficam com a mesma largura, para
            # a fila fechar como uma grelha.
            text_width = max(node.text_width for node in members_of_line)
            for node in members_of_line:
                node.text_width = text_width
                node.width = text_width + BOX_PAD_CM
            total = sum(node.width for node in members_of_line) + 0.8 * (len(members_of_line) - 1)
            cursor = -total / 2.0
            for node in members_of_line:
                node.x = cursor + node.width / 2.0
                node.y = y - sub * 1.9
                cursor += node.width + 0.8
        y -= 3.4 + (len(lines) - 1) * 1.9


def layout_sequence(diagram: Diagram) -> None:
    # Cada participante ocupa a sua coluna, e a coluna e' larga como a caixa
    # mais larga que nela cabe. Uma largura fixa deixava colunas vazias ou
    # caixas sobrepostas, conforme o numero de participantes.
    for participant in diagram.participants:
        participant.text_width = estimate_text_width(participant)
        participant.width = participant.text_width + BOX_PAD_CM
        participant.height = 0.8

    x = 0.0
    for index, participant in enumerate(diagram.participants):
        participant.x = x + participant.width / 2.0
        participant.y = 0.0
        x += participant.width
        if index < len(diagram.participants) - 1:
            x += SEQ_GAP_CM


# --------------------------------------------------------------------------
# Escrita do TikZ
# --------------------------------------------------------------------------


def preamble(diagram: Diagram) -> list[str]:
    # A tipografia e' sem serifa em todo o desenho, e a negrito. O desenho
    # e' lido de longe, e o peso ajuda a separar o rotulo do ruido do
    # fundo. O pacote helvet da o Arial em metricas, e esta em qualquer
    # TeX Live e MiKTeX, ao contrario da fonte do sistema.
    return [
        r"\definecolor{atlasyellow}{HTML}{" + YELLOW + "}",
        r"\definecolor{atlasamber}{HTML}{" + AMBER + "}",
        r"\definecolor{atlasblack}{HTML}{" + BLACK + "}",
        r"\begin{tikzpicture}[",
        "  font=\\sffamily\\bfseries\\small,",
        "  atlasgroup/.style={draw=atlasblack, fill=atlasamber, line width=0.9pt, rounded corners=4pt, inner xsep=14pt, inner ysep=26pt},",
        "  atlasgrouptext/.style={font=\\sffamily\\bfseries\\small, text=atlasblack, anchor=north west, inner sep=1pt},",
        "  atlasleaf/.style={draw=atlasblack, fill=atlasyellow, text=atlasblack, line width=0.9pt, rounded corners=4pt, align=center, inner sep=5pt, font=\\sffamily\\bfseries\\small},",
        "  atlasleafabstract/.style={atlasleaf, fill=atlasamber},",
        "  atlasflow/.style={-{Stealth[length=2.3mm,width=2mm]}, draw=atlasblack, line width=0.8pt},",
        "  atlasasync/.style={-{Stealth[length=2.3mm,width=2mm]}, draw=atlasblack, dashed, line width=0.8pt},",
        "  atlaslabel/.style={font=\\sffamily\\bfseries\\tiny, fill=white, inner sep=1.2pt, text=atlasblack},",
        "  atlasclass/.style={draw=atlasblack, fill=atlasyellow, text=atlasblack, line width=0.9pt, rounded corners=4pt, align=center, inner sep=6pt, font=\\sffamily\\bfseries\\small},",
        "  atlasclassabstract/.style={atlasclass, fill=atlasamber},",
        "  atlasuml/.style={-{Triangle[length=2.4mm,width=2.9mm]}, draw=atlasblack, line width=0.9pt},",
        "  atlascomp/.style={-{Diamond[length=2.6mm]}, draw=atlasblack, line width=0.9pt},",
        "  atlaslife/.style={draw=atlasblack, dashed, line width=0.6pt},",
        "  atlasmsg/.style={-{Stealth[length=2.2mm]}, draw=atlasblack, line width=0.8pt},",
        "  atlasmsgback/.style={-{Stealth[length=2.2mm]}, draw=atlasblack, dashed, line width=0.8pt},",
        "  atlasmsglabel/.style={font=\\sffamily\\bfseries\\scriptsize, fill=white, inner sep=1.5pt, text=atlasblack},",
        "  atlasparticipant/.style={draw=atlasblack, fill=atlasyellow, text=atlasblack, line width=0.9pt, rounded corners=4pt, align=center, inner sep=5pt, font=\\sffamily\\bfseries\\small},",
        "]",
    ]


def node_style(node: Node) -> str:
    return "atlasleafabstract" if node.cls == "abstract" else "atlasleaf"


def render_flowchart(diagram: Diagram) -> list[str]:
    layout_flowchart(diagram)
    out: list[str] = []

    for node in diagram.nodes:
        out.append(
            f"  \\node[{node_style(node)}, text width={max(node.text_width, 1.2):.2f}cm] "
            f"({node.id}) at ({node.x:.2f},{node.y:.2f}) {{{label_to_latex(node.label)}}};"
        )

    # As arestas sao desenhadas depois dos nos, para ficarem por cima.
    # Uma aresta que salta camadas e' desviada por baixo da faixa de
    # origem, senao atravessa as caixas do meio.
    #
    # Duas arestas que chegam ao mesmo no colocam o rotulo no mesmo sitio.
    # O rotulo e' deslocado ao longo da aresta, para as duas nao se
    # sobrepoarem.
    long_edge = 0
    label_slot: dict[str, int] = {}
    pair_slot: dict[frozenset, int] = {}
    for edge in diagram.edges:
        style = "atlasasync" if edge.op in ("-.->", "-.-") else "atlasflow"
        # O rotulo desloca-se ao longo da aresta. Duas arestas que chegam ao
        # mesmo no, e duas arestas entre o mesmo par de nos, precisam de
        # deslocacoes diferentes, senao os rotulos ficam um em cima do outro.
        by_target = label_slot.get(edge.dst, 0)
        pair = frozenset((edge.src, edge.dst))
        by_pair = pair_slot.get(pair, 0)
        label_slot[edge.dst] = by_target + 1
        pair_slot[pair] = by_pair + 1
        slot = max(by_target, by_pair)
        # Duas arestas entre o mesmo par de nos correm sobre a mesma linha.
        # Os rotulos de uma e de outra ficam um de cada lado, senao um tapa o
        # outro.
        side = "below" if by_pair % 2 else "above"
        # Numa aresta recta o rotulo desloca-se ao longo do troco. Numa aresta
        # desviada por baixo, o troco util e' o horizontal do meio: o
        # rotulo no vertical ficava um em cima do outro.
        pos = min(0.32 + 0.14 * slot, 0.74)
        pos_routed = min(0.5 + 0.13 * slot, 0.84)
        if edge.src == edge.dst:
            out.append(
                f"  \\draw[{style}] ({edge.src}.east) to[loop right, min distance=1.2cm] "
                f"node[atlaslabel, above] {{{label_to_latex(edge.label)}}} ({edge.src}.east);"
            )
            continue
        source = diagram.node(edge.src)
        target = diagram.node(edge.dst)
        same_layer = source is not None and target is not None and source.layer == target.layer
        same_row = source is not None and target is not None and source.row == target.row
        span = abs(source.layer - target.layer) if source and target else 0

        if same_layer:
            out.append(
                f"  \\draw[{style}] ({edge.src}) to[bend left=22] "
                f"node[atlaslabel, above] {{{label_to_latex(edge.label)}}} ({edge.dst});"
            )
        elif same_row and span <= 1:
            out.append(
                f"  \\draw[{style}] ({edge.src}) -- "
                f"node[atlaslabel, {side}, pos={pos:.2f}] "
                f"{{{label_to_latex(edge.label)}}} ({edge.dst});"
            )
        else:
            # A aresta salta camadas ou faixas. O caminho passa por baixo da
            # faixa de origem, senao atravessa as caixas do meio.
            band = diagram.band_bottom.get(source.band, source.y - 0.5)
            route = band - 0.6 - 1.0 * long_edge
            long_edge += 1
            drop = source.y - route
            out.append(
                f"  \\draw[{style}] ({edge.src}.south) -- "
                f"([yshift=-{drop:.2f}cm]{edge.src}.south) |- "
                f"([yshift=-{drop:.2f}cm]{edge.dst}.south) -- ({edge.dst}.south) "
                f"node[atlaslabel, pos={pos_routed:.2f}, below] {{{label_to_latex(edge.label)}}};"
            )

    # As caixas de agrupamento vao para a camada de fundo, com o texto no topo.
    for group in diagram.groups:
        members = [name for name in group.members if diagram.node(name) is not None]
        if not members:
            continue
        joined = " ".join(f"({name})" for name in members)
        out.append("  \\begin{scope}[on background layer]")
        out.append(f"    \\node[atlasgroup, fit={joined}] (grp_{group.id}) {{}};")
        out.append("  \\end{scope}")
        out.append(
            f"  \\node[atlasgrouptext] at "
            f"([xshift=7pt,yshift=-5pt]grp_{group.id}.north west) "
            f"{{{label_to_latex(group.label)}}};"
        )
    return out


def render_classdiagram(diagram: Diagram) -> list[str]:
    layout_classdiagram(diagram)
    out: list[str] = []

    for node in diagram.nodes:
        style = "atlasclassabstract" if node.cls == "abstract" else "atlasclass"
        out.append(
            f"  \\node[{style}, text width={max(node.text_width, 1.2):.2f}cm] "
            f"({node.id}) at ({node.x:.2f},{node.y:.2f}) {{{label_to_latex(node.label)}}};"
        )

    for edge in diagram.edges:
        label = label_to_latex(edge.label)
        source = diagram.node(edge.src)
        target = diagram.node(edge.dst)
        backwards = (
            source is not None and target is not None
            and target.x < source.x - 0.01
        )
        if edge.op == "<|--":
            out.append(
                f"  \\draw[atlasuml] ({edge.src}.south) -- ({edge.dst}.north);"
            )
        elif edge.op == "*--":
            if label:
                out.append(
                    f"  \\draw[atlascomp] ({edge.src}.south) -- "
                    f"node[atlaslabel, above, pos=0.35] {{{label}}} ({edge.dst}.north);"
                )
            else:
                out.append(
                    f"  \\draw[atlascomp] ({edge.src}.south) -- ({edge.dst}.north);"
                )
        elif edge.op == "-->":
            if backwards:
                # A associacao vai de tras para a frente. A linha reta
                # atravessava as caixas do meio, por isso passa por baixo.
                bottom = min(node.y - node.height / 2.0 for node in diagram.nodes)
                route = bottom - 0.6
                drop = source.y - route
                out.append(
                    f"  \\draw[atlasflow] ({edge.src}.south) -- "
                    f"([yshift=-{drop:.2f}cm]{edge.src}.south) |- "
                    f"([yshift=-{drop:.2f}cm]{edge.dst}.south) -- ({edge.dst}.south) "
                    f"node[atlaslabel, pos=0.5, below] {{{label}}};"
                )
            else:
                out.append(
                    f"  \\draw[atlasflow] ({edge.src}) -- "
                    f"node[atlaslabel, above] {{{label}}} ({edge.dst});"
                )
        else:
            out.append(
                f"  \\draw[atlasasync] ({edge.src}) -- "
                f"node[atlaslabel, above] {{{label}}} ({edge.dst});"
            )
    return out


def render_sequence(diagram: Diagram) -> list[str]:
    layout_sequence(diagram)
    out: list[str] = []

    for participant in diagram.participants:
        out.append(
            f"  \\node[atlasparticipant, text width={max(participant.text_width, 1.2):.2f}cm] "
            f"({participant.id}) at ({participant.x:.2f},{participant.y:.2f}) "
            f"{{{label_to_latex(participant.label)}}};"
        )

    depth = len(diagram.messages)
    bottom = -(1.25 + max(depth, 1) * SEQ_ROW_CM)
    for participant in diagram.participants:
        out.append(
            f"  \\draw[atlaslife] ({participant.id}.south) -- "
            f"({participant.x:.2f},{bottom:.2f});"
        )

    for index, message in enumerate(diagram.messages):
        y = -1.25 - index * SEQ_ROW_CM
        text = label_to_latex(message.text)
        style = "atlasmsgback" if message.op.startswith("--") else "atlasmsg"
        if message.src == message.dst:
            out.append(
                f"  \\draw[{style}] ({message.src}.east |- 0,{y:.2f}) "
                f"to[loop right, min distance=1.4cm] "
                f"node[atlasmsglabel, above] {{{text}}} "
                f"({message.src}.east |- 0,{y:.2f});"
            )
            continue
        # As duas pontas ficam na altura da mensagem: o `|-` toma o x do
        # participante e o y da linha da mensagem. Sem isso, todas as
        # mensagens caem na mesma linha, em cima da fila dos participantes.
        out.append(
            f"  \\draw[{style}] ({message.src}.east |- 0,{y:.2f}) -- "
            f"node[atlasmsglabel, above] {{{text}}} "
            f"({message.dst}.west |- 0,{y:.2f});"
        )
    return out


def layout(diagram: Diagram) -> None:
    """Calcula as posicoes, conforme o tipo de diagrama."""
    if diagram.kind == "sequenceDiagram":
        layout_sequence(diagram)
    elif diagram.kind == "classDiagram":
        layout_classdiagram(diagram)
    else:
        layout_flowchart(diagram)


def extent(diagram: Diagram) -> tuple[float, float]:
    """Largura e altura do desenho em cm, para conferir que cabe na pagina."""
    layout(diagram)
    if not diagram.nodes and not diagram.participants:
        return (0.0, 0.0)
    if diagram.kind == "sequenceDiagram":
        left = min(p.x - p.width / 2.0 for p in diagram.participants)
        right = max(p.x + p.width / 2.0 for p in diagram.participants)
        bottom = -(1.25 + max(len(diagram.messages), 1) * SEQ_ROW_CM)
        return (right - left, abs(bottom) + 1.2)
    left = min(node.x - node.width / 2.0 for node in diagram.nodes)
    right = max(node.x + node.width / 2.0 for node in diagram.nodes)
    top = max(node.y + node.height / 2.0 for node in diagram.nodes)
    bottom = min(node.y - node.height / 2.0 for node in diagram.nodes)
    # As caixas de agrupamento engordam o desenho, e as arestas que saltam
    # camadas sobem acima de tudo.
    top += 0.5 + 0.55 * max(0, len(diagram.edges) - 12)
    return (right - left + 1.0, top - bottom + 1.0)


def render(diagram: Diagram) -> str:
    if diagram.kind == "sequenceDiagram":
        body = render_sequence(diagram)
    elif diagram.kind == "classDiagram":
        body = render_classdiagram(diagram)
    else:
        body = render_flowchart(diagram)

    lines = preamble(diagram)
    lines.extend(body)
    lines.append(r"\end{tikzpicture}")
    return "\n".join(lines) + "\n"


# --------------------------------------------------------------------------
# Programa
# --------------------------------------------------------------------------


def git_blob_hash(path: Path) -> str:
    """Hash do blob no git. Liga o .tex gerado ao .mmd de origem."""
    try:
        result = subprocess.run(
            ["git", "hash-object", str(path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return hashlib.sha1(path.read_bytes()).hexdigest()


def convert(source: Path) -> str:
    text = source.read_text(encoding="utf-8", errors="replace")
    diagram = parse(text)
    body = render(diagram)
    header = [
        "% Gerado por tools/mmd2tikz.py a partir de "
        f"{source.as_posix()}. Nao editar a mao.",
        f"% git blob da fonte: {git_blob_hash(source)}",
        "",
    ]
    return "\n".join(header) + body


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Mermaid do Catlas para TikZ")
    parser.add_argument("--in", dest="source", required=True, help="ficheiro .mmd ou pasta")
    parser.add_argument("--out", dest="target", required=True, help="pasta de saida ou '-'")
    parser.add_argument("--report", action="store_true",
                        help="imprime a largura e a altura de cada desenho")
    args = parser.parse_args(argv[1:])

    source = Path(args.source)
    target = Path(args.target)

    if source.is_dir():
        files = sorted(source.glob("*.mmd"))
    else:
        files = [source]
    if not files:
        print(f"nenhum .mmd em {source}")
        return 1

    for mmd in files:
        tex = convert(mmd)
        if args.report:
            diagram = parse(mmd.read_text(encoding="utf-8", errors="replace"))
            width, height = extent(diagram)
            print(f"{mmd.stem}: {width:.1f} x {height:.1f} cm")
        if str(target) == "-":
            sys.stdout.write(tex)
            continue
        target.mkdir(parents=True, exist_ok=True)
        destination = target / (mmd.stem + ".tex")
        destination.write_text(tex, encoding="utf-8")
        print(f"{mmd.name} -> {destination.as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
