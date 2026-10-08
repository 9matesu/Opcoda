"""Extrai e reduz os ficheiros de fonte que o Opcoda embebe.

Porquê reduzir em vez de embutir os TTF inteiros: Inter tem 408 KB por peso, e os
dois pesos que a interface usa somam 816 KB. A interface nao usa mais de 120
caracteres diferentes, e reduzir so' a esses dá um resultado visualmente
identico.

O conjunto e' fixo e declarado aqui em vez de ser construido a partir do codigo,
porque o que a interface escreve e' um conjunto conhecido: nomes de ficheiro, um
endereco hexadecimal, `bits/byte`, dB, e as palavras do estado. Qualquer caractere
fora do conjunto sai como um caixote, e ver o ecrã e' como se apanha.

Uso:
    python tools/vendor_fonts.py <pasta> [<pasta> ...]

Cada pasta e' procurada por todos os ficheiros de WANTED.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

# O que a interface escreve, com folga. Inclui latin-1 supplement para os acentos
# que um nome de ficheiro pode ter, e os sinais que aparecem em leituras.
CHARACTERS = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    " .,:;!?/\\|-_+=*&#%@"
    "()[]{}<>'\"`~^$"
    "\u00b0\u00b1\u00b7\u00d7\u00f7"
    "\u00c0\u00c1\u00c2\u00c3\u00c4\u00c5\u00c7\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf"
    "\u00d2\u00d3\u00d4\u00d5\u00d6\u00d8\u00d9\u00da\u00db\u00dc\u00dd\u00df"
    "\u00e0\u00e1\u00e2\u00e3\u00e4\u00e5\u00e7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef"
    "\u00f2\u00f3\u00f4\u00f5\u00f6\u00f8\u00f9\u00fa\u00fb\u00fc\u00fd\u00ff"
    "\u2013\u2014\u2018\u2019\u201c\u201d\u2026\u2192\u25b8"
)

# (fonte de origem, nome de saida)
#
# **So' os pesos que a interface usa.** Nao e' uma lista do que existe na fonte, e'
# a lista do que o codigo pede: peso medio para texto e leituras, semi-negrito
# para rotulos e titulos. A monoespacada saiu — a interface e' Inter em tudo.
#
# Inter e' **Display** e nao o corte normal, e a razao e' o tamanho. InterDisplay e'
# a variante com altura de x e contraste concebidos para texto pequeno — 9 a 11 px,
# que e' exactamente onde esta interface passa a vida. A 10 px, InterDisplay le-se
# melhor do que Inter, e a letra e' mais aberta sem ser maior.
WANTED = [
    ("extras/ttf/InterDisplay-Medium.ttf", "InterDisplay-Medium.ttf"),
    ("extras/ttf/InterDisplay-SemiBold.ttf", "InterDisplay-SemiBold.ttf"),
]

# (nameID 1 familia, nameID 2 estilo, nameID 6 postscript)
#
# **Isto nao e' cosmetica.** Os TTF originais do Inter Display trazem o MESMO nome
# PostScript, "Inter Display", nos dois pesos, e o nameID 2 diz "Regular" em todos
# os quatro ficheiros. A busca de tipos do JUCE e' por familia e estilo, e dois
# ficheiros com o mesmo nome colidem: o texto sem serlinha sairia todo no mesmo
# peso, sem aviso nenhum. Um nome errado aqui e' um defeite silencioso, e por isso
# se reescreve em vez de confiar no que a fonte traz.
NAMING = {
    "InterDisplay-Medium.ttf": ("Inter Display", "Medium", "InterDisplay-Medium"),
    "InterDisplay-SemiBold.ttf": ("Inter Display", "SemiBold", "InterDisplay-SemiBold"),
}

OPTIONS = subset.Options()
OPTIONS.layout_features = ["*"]
OPTIONS.name_IDs = ["*"]
OPTIONS.name_legacy = True
OPTIONS.notdef_outline = True
OPTIONS.recalc_bounds = True
OPTIONS.drop_tables += ["DSIG"]
# hinted: nao reduzir. A hinting e' o que faz uma linha de 9 px nao tremer, e um
# ficheiro de 60 KB que treme e' pior do que um de 400 KB que nao treme.
OPTIONS.hinting = True


def reduce_font(source: Path, target: Path) -> tuple[int, int]:
    font = TTFont(source)
    before = source.stat().st_size

    options = subset.Options()
    options.layout_features = ["*"]
    options.name_IDs = ["*"]
    options.name_legacy = True
    options.notdef_outline = True
    options.recalc_bounds = True
    options.hinting = True
    options.drop_tables += ["DSIG"]

    subsetter = subset.Subsetter(options=options)
    subsetter.populate(text=CHARACTERS)
    subsetter.subset(font)

    # Reescrever os nomes. Ver NAMING acima para a razao.
    family, style, postscript = NAMING[target.name]
    full = f"{family} {style}" if style != "Regular" else family

    for record in font["name"].names:
        replacement = {1: family, 2: style, 4: full, 6: postscript}.get(record.nameID)
        if replacement is not None:
            record.string = replacement.encode(record.getEncoding())

    font.save(target)
    font.close()
    return before, target.stat().st_size


def locate(roots: list[Path], relative: str) -> Path | None:
    """Procura `relative` em qualquer um dos raizes.

    Inter e JetBrains Mono vem de arquivos differentes, com caminhosInternos
    diferentes, e receber dois argumentos e' mais honesto do que inventar um layout
    unico e pedir a quem corre que o monte.
    """
    for root in roots:
        candidate = root / relative
        if candidate.exists():
            return candidate
    return None


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    roots = [Path(argument) for argument in sys.argv[1:]]
    target_root = Path(__file__).resolve().parent.parent / "resources" / "fonts"
    target_root.mkdir(parents=True, exist_ok=True)

    total_before = 0
    total_after = 0
    missing: list[str] = []

    for relative, name in WANTED:
        source = locate(roots, relative)
        if source is None:
            missing.append(relative)
            continue

        before, after = reduce_font(source, target_root / name)
        total_before += before
        total_after += after
        print(f"  {name:<32} {before / 1024:7.0f} KB -> {after / 1024:6.0f} KB")

    print(f"\n  {'total':<32} {total_before / 1024:7.0f} KB -> {total_after / 1024:6.0f} KB")

    if missing:
        print("\nfontes em falta:")
        for relative in missing:
            print(f"  {relative}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())