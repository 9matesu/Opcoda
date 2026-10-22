import sys
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE

def create_mochila_deck(output_path):
    prs = Presentation()
    # Exact Canva widescreen standard (20.0 x 11.25 inches)
    prs.slide_width = Inches(20.0)
    prs.slide_height = Inches(11.25)

    # Design System Color Tokens
    BG_COLOR = RGBColor(0xF4, 0xF5, 0xF6)       # Off-white background
    SURFACE_COLOR = RGBColor(0xFF, 0xFF, 0xFF)  # Pure white card surface
    TEXT_COLOR = RGBColor(0x12, 0x15, 0x18)     # Near Black
    MUTED_COLOR = RGBColor(0x5A, 0x60, 0x65)    # Slate / Gray
    ACCENT_COLOR = RGBColor(0xFF, 0x9A, 0x00)   # Opcoda Amber (#FF9A00)
    BORDER_COLOR = RGBColor(0xD8, 0xDC, 0xDE)   # Divider gray
    DARK_BOX = RGBColor(0x0E, 0x10, 0x13)       # Terminal Dark
    HIGHLIGHT_BG = RGBColor(0xFF, 0xFD, 0xF9)   # Warm highlighted card
    GREEN_ACCENT = RGBColor(0x2E, 0x7D, 0x32)   # Success / Feasible
    WHITE = RGBColor(0xFF, 0xFF, 0xFF)

    blank_layout = prs.slide_layouts[6]
    TOTAL_SLIDES = 23

    def set_background(slide):
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, prs.slide_width, prs.slide_height)
        bg.fill.solid()
        bg.fill.fore_color.rgb = BG_COLOR
        bg.line.fill.background()
        return bg

    def add_header(slide, number_str, section_str):
        # Slide number
        num_box = slide.shapes.add_textbox(Inches(1.3), Inches(0.7), Inches(1.4), Inches(0.6))
        tf = num_box.text_frame
        p = tf.paragraphs[0]
        p.text = number_str
        p.font.name = "Arial"
        p.font.size = Pt(22)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR

        # Section tag
        sec_box = slide.shapes.add_textbox(Inches(2.5), Inches(0.72), Inches(12.0), Inches(0.6))
        tf = sec_box.text_frame
        p = tf.paragraphs[0]
        p.text = section_str.upper()
        p.font.name = "Arial"
        p.font.size = Pt(14)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

        # Top divider line
        line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(1.45), Inches(17.41), Inches(0.025))
        line.fill.solid()
        line.fill.fore_color.rgb = TEXT_COLOR
        line.line.fill.background()

    def add_footer(slide, number_str):
        # Footer divider line
        line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(10.15), Inches(17.41), Inches(0.02))
        line.fill.solid()
        line.fill.fore_color.rgb = BORDER_COLOR
        line.line.fill.background()

        # Left tag
        fb = slide.shapes.add_textbox(Inches(1.3), Inches(10.25), Inches(10.0), Inches(0.5))
        p = fb.text_frame.paragraphs[0]
        p.text = "OPCODA · PROBLEMA DA MOCHILA COM TÊMPERA SIMULADA"
        p.font.name = "Arial"
        p.font.size = Pt(13)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

        # Right count
        fb_r = slide.shapes.add_textbox(Inches(15.7), Inches(10.25), Inches(3.0), Inches(0.5))
        p = fb_r.text_frame.paragraphs[0]
        p.alignment = PP_ALIGN.RIGHT
        p.text = f"{number_str} / {TOTAL_SLIDES:02d}"
        p.font.name = "Arial"
        p.font.size = Pt(13)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

    def add_title(slide, title_text):
        tb = slide.shapes.add_textbox(Inches(1.3), Inches(1.85), Inches(17.41), Inches(1.2))
        tf = tb.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        p.text = title_text
        p.font.name = "Arial"
        p.font.size = Pt(38)
        p.font.bold = True
        p.font.color.rgb = TEXT_COLOR

    def add_bullets(slide, items, top_inch=3.4, left_inch=1.3, width_inch=17.41, font_size=23, spacing=26):
        tb = slide.shapes.add_textbox(Inches(left_inch), Inches(top_inch), Inches(width_inch), Inches(5.8))
        tf = tb.text_frame
        tf.word_wrap = True
        for i, text in enumerate(items):
            p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
            p.text = f"■   {text}"
            p.font.name = "Arial"
            p.font.size = Pt(font_size)
            p.font.bold = False
            p.font.color.rgb = TEXT_COLOR
            p.space_after = Pt(spacing)

    # =============================================================
    # SLIDE 1 — CAPA
    # =============================================================
    s1 = prs.slides.add_slide(blank_layout)
    set_background(s1)
    add_header(s1, "01", "FATEC Cruzeiro · Programação Linear / Otimização")

    tag1 = s1.shapes.add_textbox(Inches(1.3), Inches(2.4), Inches(15.0), Inches(0.6))
    p = tag1.text_frame.paragraphs[0]
    p.text = "TRABALHO ACADÊMICO DE OTIMIZAÇÃO · PROJETO OPCODA"
    p.font.name = "Arial"
    p.font.size = Pt(16)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    t1 = s1.shapes.add_textbox(Inches(1.3), Inches(3.0), Inches(17.4), Inches(2.2))
    tf1 = t1.text_frame
    tf1.word_wrap = True
    p1 = tf1.paragraphs[0]
    p1.text = "Aplicação de Têmpera Simulada\nno Problema da Mochila"
    p1.font.name = "Arial"
    p1.font.size = Pt(46)
    p1.font.bold = True
    p1.font.color.rgb = TEXT_COLOR

    sub1 = s1.shapes.add_textbox(Inches(1.3), Inches(5.4), Inches(17.4), Inches(1.0))
    p_sub = sub1.text_frame.paragraphs[0]
    p_sub.text = "Uma aplicação experimental no contexto do Opcoda"
    p_sub.font.name = "Arial"
    p_sub.font.size = Pt(26)
    p_sub.font.color.rgb = MUTED_COLOR

    # Card autores
    card1 = s1.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(6.8), Inches(10.5), Inches(2.2))
    card1.fill.solid()
    card1.fill.fore_color.rgb = SURFACE_COLOR
    card1.line.color.rgb = BORDER_COLOR
    card1.line.width = Pt(1)

    bar1 = s1.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(6.8), Inches(0.18), Inches(2.2))
    bar1.fill.solid()
    bar1.fill.fore_color.rgb = ACCENT_COLOR
    bar1.line.fill.background()

    aut1 = s1.shapes.add_textbox(Inches(1.8), Inches(7.05), Inches(9.5), Inches(1.8))
    p_aut = aut1.text_frame.paragraphs[0]
    p_aut.text = "Mateus Costa · Rafael Elisei"
    p_aut.font.name = "Arial"
    p_aut.font.size = Pt(24)
    p_aut.font.bold = True
    p_aut.font.color.rgb = TEXT_COLOR
    p_aut2 = aut1.text_frame.add_paragraph()
    p_aut2.text = "5º semestre · Análise e Desenvolvimento de Sistemas\nFATEC Cruzeiro"
    p_aut2.font.name = "Arial"
    p_aut2.font.size = Pt(18)
    p_aut2.font.color.rgb = MUTED_COLOR
    p_aut2.space_before = Pt(8)

    add_footer(s1, "01")

    # =============================================================
    # SLIDE 2 — OPCODA
    # =============================================================
    s2 = prs.slides.add_slide(blank_layout)
    set_background(s2)
    add_header(s2, "02", "Fundamentação · Contexto")
    add_title(s2, "O que é o Opcoda?")
    add_bullets(s2, [
        "Instrumento virtual experimental para processamento sonoro",
        "Arquivos executáveis (.exe) utilizados como matéria-prima de áudio",
        "Dados binários convertidos diretamente para geração sonora",
        "Síntese granular como etapa principal de processamento acústico"
    ], top_inch=3.3, font_size=23, spacing=24)

    # Pipeline visual horizontal
    nodes_s2 = ["ARQUIVO EXECUTÁVEL", "→", "DADOS BINÁRIOS", "→", "BLOCOS", "→", "GRÃOS", "→", "SOM"]
    x = 1.3
    for item in nodes_s2:
        if item == "→":
            tb = s2.shapes.add_textbox(Inches(x), Inches(7.9), Inches(0.6), Inches(0.8))
            p = tb.text_frame.paragraphs[0]
            p.text = "→"
            p.font.name = "Arial"
            p.font.size = Pt(26)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR
            x += 0.65
        else:
            is_accent = (item == "SOM")
            w_box = 3.1 if "ARQUIVO" in item else (2.7 if "DADOS" in item else 2.1)
            box = s2.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(7.7), Inches(w_box), Inches(1.1))
            box.fill.solid()
            box.fill.fore_color.rgb = ACCENT_COLOR if is_accent else SURFACE_COLOR
            box.line.color.rgb = ACCENT_COLOR if is_accent else TEXT_COLOR
            box.line.width = Pt(1.5)
            p = box.text_frame.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER
            p.text = item
            p.font.name = "Arial"
            p.font.size = Pt(17)
            p.font.bold = True
            p.font.color.rgb = WHITE if is_accent else TEXT_COLOR
            x += (w_box + 0.15)

    add_footer(s2, "02")

    # =============================================================
    # SLIDE 3 — O PROBLEMA
    # =============================================================
    s3 = prs.slides.add_slide(blank_layout)
    set_background(s3)
    add_header(s3, "03", "Problema · Otimização")
    add_title(s3, "Onde entra a otimização?")
    add_bullets(s3, [
        "O arquivo executável gera centenas de blocos candidatos",
        "Nem todos os blocos precisam ou devem ser selecionados",
        "Existe uma capacidade finita e limitada de processamento/tempo",
        "É necessário selecionar uma combinação ótima de blocos sob restrição"
    ], top_inch=3.3, left_inch=1.3, width_inch=10.0, font_size=23, spacing=24)

    # Card visual Mochila com capacidade
    c3 = s3.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(12.0), Inches(3.3), Inches(6.7), Inches(5.8))
    c3.fill.solid()
    c3.fill.fore_color.rgb = SURFACE_COLOR
    c3.line.color.rgb = ACCENT_COLOR
    c3.line.width = Pt(2)

    bar3 = s3.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(12.0), Inches(3.3), Inches(6.7), Inches(0.9))
    bar3.fill.solid()
    bar3.fill.fore_color.rgb = ACCENT_COLOR
    bar3.line.fill.background()
    p = bar3.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "MOCHILA (CAPACIDADE = C)"
    p.font.name = "Arial"
    p.font.size = Pt(18)
    p.font.bold = True
    p.font.color.rgb = WHITE

    tb3 = s3.shapes.add_textbox(Inches(12.4), Inches(4.5), Inches(6.0), Inches(4.3))
    tf3 = tb3.text_frame
    tf3.word_wrap = True
    items_mochila = [
        "Bloco 1 (peso w₁, valor v₁)  →  [ Selecionar? ]",
        "Bloco 2 (peso w₂, valor v₂)  →  [ Selecionar? ]",
        "Bloco 3 (peso w₃ valor v₃)  →  [ Selecionar? ]",
        "Bloco 4 (peso w₄, valor v₄)  →  [ Selecionar? ]",
        "...",
        "Restrição: Peso Total ≤ Capacidade C"
    ]
    for i, it in enumerate(items_mochila):
        p = tf3.paragraphs[0] if i == 0 else tf3.add_paragraph()
        p.text = it
        p.font.name = "Arial"
        p.font.size = Pt(16)
        p.font.bold = (i == len(items_mochila) - 1)
        p.font.color.rgb = ACCENT_COLOR if (i == len(items_mochila) - 1) else TEXT_COLOR
        p.space_after = Pt(12)

    add_footer(s3, "03")

    # =============================================================
    # SLIDE 4 — PROBLEMA DA MOCHILA
    # =============================================================
    s4 = prs.slides.add_slide(blank_layout)
    set_background(s4)
    add_header(s4, "04", "Fundamentação Teórica")
    add_title(s4, "Problema da Mochila (Knapsack Problem)")

    # 4 cards de itens horizontais
    itens = [
        ("ITEM 1", "peso = w₁", "valor = v₁"),
        ("ITEM 2", "peso = w₂", "valor = v₂"),
        ("ITEM 3", "peso = w₃", "valor = v₃"),
        ("ITEM 4", "peso = w₄", "valor = v₄"),
    ]
    for i, (nom, p_str, v_str) in enumerate(itens):
        bx = s4.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3 + i * 4.45), Inches(3.2), Inches(4.1), Inches(1.8))
        bx.fill.solid()
        bx.fill.fore_color.rgb = SURFACE_COLOR
        bx.line.color.rgb = BORDER_COLOR
        bx.line.width = Pt(1.5)
        tb = bx.text_frame
        p = tb.paragraphs[0]
        p.text = nom
        p.font.name = "Arial"
        p.font.size = Pt(18)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR
        p2 = tb.add_paragraph()
        p2.text = f"{p_str}   |   {v_str}"
        p2.font.size = Pt(16)
        p2.font.color.rgb = TEXT_COLOR
        p2.space_before = Pt(8)

    # Seta para baixo e mochila
    seta_down = s4.shapes.add_textbox(Inches(9.5), Inches(5.1), Inches(1.5), Inches(0.8))
    p = seta_down.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "↓"
    p.font.size = Pt(36)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    mochila_box = s4.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(5.9), Inches(17.41), Inches(1.2))
    mochila_box.fill.solid()
    mochila_box.fill.fore_color.rgb = HIGHLIGHT_BG
    mochila_box.line.color.rgb = ACCENT_COLOR
    mochila_box.line.width = Pt(2)
    p = mochila_box.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "MOCHILA COM CAPACIDADE MÁXIMA = C"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    add_bullets(s4, [
        "Cada item possui um peso associado e um valor associado",
        "Cada item pode ser selecionado (1) ou descartado (0)",
        "A capacidade C limita estritamente o peso total acumulado",
        "O objetivo clássico é maximizar o valor total dos itens selecionados"
    ], top_inch=7.4, font_size=21, spacing=14)

    add_footer(s4, "04")

    # =============================================================
    # SLIDE 5 — MODELO MATEMÁTICO
    # =============================================================
    s5 = prs.slides.add_slide(blank_layout)
    set_background(s5)
    add_header(s5, "05", "Modelagem Matemática")
    add_title(s5, "Formulação do Modelo")

    # Caixa da formulação grande
    f_box = s5.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(2.9))
    f_box.fill.solid()
    f_box.fill.fore_color.rgb = SURFACE_COLOR
    f_box.line.color.rgb = TEXT_COLOR
    f_box.line.width = Pt(2.5)
    tf5 = f_box.text_frame
    tf5.word_wrap = True

    p = tf5.paragraphs[0]
    p.text = "max   ∑ (vᵢ · xᵢ)"
    p.font.name = "Arial"
    p.font.size = Pt(38)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR
    p.space_after = Pt(14)

    p2 = tf5.add_paragraph()
    p2.text = "sujeito a:     ∑ (wᵢ · xᵢ)  ≤  C"
    p2.font.name = "Arial"
    p2.font.size = Pt(32)
    p2.font.bold = True
    p2.font.color.rgb = TEXT_COLOR
    p2.space_after = Pt(14)

    p3 = tf5.add_paragraph()
    p3.text = "xᵢ  ∈  { 0, 1 }      para todo i = 1, ..., n"
    p3.font.name = "Arial"
    p3.font.size = Pt(26)
    p3.font.color.rgb = MUTED_COLOR

    # Legenda explicativa
    t_leg = s5.shapes.add_textbox(Inches(1.3), Inches(6.5), Inches(17.41), Inches(3.0))
    tf_leg = t_leg.text_frame
    legendas = [
        "vᵢ = valor do item i (critério de relevância ou qualidade)",
        "wᵢ = peso do item i (tamanho do bloco ou custo de processamento)",
        "C = capacidade total suportada pela mochila",
        "xᵢ = variável de decisão binária (1 = selecionado, 0 = descartado)"
    ]
    for i, leg in enumerate(legendas):
        p = tf_leg.paragraphs[0] if i == 0 else tf_leg.add_paragraph()
        p.text = f"■   {leg}"
        p.font.name = "Arial"
        p.font.size = Pt(22)
        p.font.color.rgb = TEXT_COLOR
        p.space_after = Pt(16)

    add_footer(s5, "05")

    # =============================================================
    # SLIDE 6 — MOCHILA NO OPCODA
    # =============================================================
    s6 = prs.slides.add_slide(blank_layout)
    set_background(s6)
    add_header(s6, "06", "Modelagem · Aplicação")
    add_title(s6, "Como isso se relaciona ao Opcoda?")

    # Tabela / correspondência em 4 colunas visuais
    mapeamento = [
        ("CONCEITO NO OPCODA", "BLOCO / GRÃO", "CARACTERÍSTICA", "TAMANHO / CUSTO", "LIMITE DEFINIDO"),
        ("ANALOGIA NA MOCHILA", "ITEM CANDIDATO", "VALOR (vᵢ)", "PESO (wᵢ)", "CAPACIDADE (C)")
    ]

    table_shape6 = s6.shapes.add_table(5, 2, Inches(1.3), Inches(3.2), Inches(17.41), Inches(4.5))
    t6 = table_shape6.table
    t6.columns[0].width = Inches(8.7)
    t6.columns[1].width = Inches(8.71)

    headers6 = ["Elemento no Opcoda (Áudio & Binário)", "Correspondência no Problema da Mochila"]
    data6 = [
        ("Bloco extraído dos dados binários / Grão sonoro", "Item candidato à seleção"),
        ("Relevância / densidade / característica do bloco", "Valor associado (vᵢ) a ser maximizado"),
        ("Tamanho do bloco de bytes / custo computacional", "Peso associado (wᵢ) acumulado"),
        ("Tempo máximo de áudio / buffer de grãos disponível", "Capacidade total da mochila (C)")
    ]

    for col_idx, h in enumerate(headers6):
        cell = t6.cell(0, col_idx)
        cell.fill.solid()
        cell.fill.fore_color.rgb = TEXT_COLOR
        p = cell.text_frame.paragraphs[0]
        p.text = h
        p.font.name = "Arial"
        p.font.size = Pt(17)
        p.font.bold = True
        p.font.color.rgb = WHITE

    for row_idx, row in enumerate(data6):
        for col_idx, val in enumerate(row):
            cell = t6.cell(row_idx + 1, col_idx)
            cell.fill.solid()
            cell.fill.fore_color.rgb = SURFACE_COLOR
            p = cell.text_frame.paragraphs[0]
            p.text = val
            p.font.name = "Arial"
            p.font.size = Pt(19)
            p.font.bold = (col_idx == 1)
            p.font.color.rgb = ACCENT_COLOR if col_idx == 1 else TEXT_COLOR

    c6 = s6.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(8.2), Inches(17.41), Inches(1.3))
    c6.fill.solid()
    c6.fill.fore_color.rgb = HIGHLIGHT_BG
    c6.line.color.rgb = ACCENT_COLOR
    c6.line.width = Pt(2)
    p = c6.text_frame.paragraphs[0]
    p.text = "“A otimização busca a melhor combinação de blocos sonoros dentro da capacidade disponível.”"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = c6.text_frame.add_paragraph()
    p2.text = "Nota: Os pesos e valores são apresentados como modelo experimental a ser calibrado."
    p2.font.size = Pt(15)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(6)

    add_footer(s6, "06")

    # =============================================================
    # SLIDE 7 — TÊMPERA SIMULADA
    # =============================================================
    s7 = prs.slides.add_slide(blank_layout)
    set_background(s7)
    add_header(s7, "07", "Metaheurística · Fundamentação")
    add_title(s7, "O que é Têmpera Simulada?")
    add_bullets(s7, [
        "Metaheurística probabilística para problemas de otimização combinatória",
        "Inspirada no processo físico de resfriamento e recozimento de materiais",
        "Explora o espaço de soluções aceitando soluções piores temporariamente",
        "Reduz progressivamente a temperatura ao longo das iterações do algoritmo"
    ], top_inch=3.4, font_size=24, spacing=26)

    c7 = s7.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(7.4), Inches(17.41), Inches(2.0))
    c7.fill.solid()
    c7.fill.fore_color.rgb = SURFACE_COLOR
    c7.line.color.rgb = BORDER_COLOR
    c7.line.width = Pt(1)

    bar7 = s7.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(7.4), Inches(0.18), Inches(2.0))
    bar7.fill.solid()
    bar7.fill.fore_color.rgb = ACCENT_COLOR
    bar7.line.fill.background()

    tb7 = s7.shapes.add_textbox(Inches(1.8), Inches(7.7), Inches(16.5), Inches(1.5))
    p = tb7.text_frame.paragraphs[0]
    p.text = "“Permitir soluções piores ajuda a escapar de ótimos locais.”"
    p.font.name = "Arial"
    p.font.size = Pt(26)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb7.text_frame.add_paragraph()
    p2.text = "Referência seminal: Kirkpatrick, Gelatt e Vecchi (1983)"
    p2.font.size = Pt(17)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(8)

    add_footer(s7, "07")

    # =============================================================
    # SLIDE 8 — REPRESENTAÇÃO DA SOLUÇÃO
    # =============================================================
    s8 = prs.slides.add_slide(blank_layout)
    set_background(s8)
    add_header(s8, "08", "Modelagem · Codificação")
    add_title(s8, "Como representar uma solução?")

    # Vetor binário em destaque
    v_box = s8.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(1.8))
    v_box.fill.solid()
    v_box.fill.fore_color.rgb = SURFACE_COLOR
    v_box.line.color.rgb = TEXT_COLOR
    v_box.line.width = Pt(2)
    p = v_box.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "S  =  [  1 ,   0 ,   1 ,   1 ,   0 ,   0 ,   1  ]"
    p.font.name = "Arial"
    p.font.size = Pt(40)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    add_bullets(s8, [
        "1 = item / bloco selecionado para a mochila",
        "0 = item / bloco não selecionado (descartado)",
        "Cada posição do vetor corresponde a um item candidato fixo",
        "A solução formal é um vetor binário de decisão de dimensão n"
    ], top_inch=5.4, font_size=23, spacing=20)

    # Condições
    c8 = s8.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(8.3), Inches(17.41), Inches(1.2))
    c8.fill.solid()
    c8.fill.fore_color.rgb = HIGHLIGHT_BG
    c8.line.color.rgb = BORDER_COLOR
    p = c8.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Peso Total(S) = ∑ (wᵢ · xᵢ) ≤ C         |         Valor Total(S) = ∑ (vᵢ · xᵢ)"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    add_footer(s8, "08")

    # =============================================================
    # SLIDE 9 — SOLUÇÃO INICIAL
    # =============================================================
    s9 = prs.slides.add_slide(blank_layout)
    set_background(s9)
    add_header(s9, "09", "Inicialização · Solução S₀")
    add_title(s9, "Solução inicial")

    # Exemplo S0
    s0_box = s9.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(2.2))
    s0_box.fill.solid()
    s0_box.fill.fore_color.rgb = SURFACE_COLOR
    s0_box.line.color.rgb = BORDER_COLOR
    s0_box.line.width = Pt(1.5)
    tb = s0_box.text_frame
    p = tb.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "S₀  =  [  1 ,   0 ,   1 ,   0 ,   0 ,   1  ]"
    p.font.name = "Arial"
    p.font.size = Pt(36)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    p2 = tb.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "Peso(S₀) = w₁ + w₃ + w₆  ≤  C           |           Valor(S₀) = v₁ + v₃ + v₆"
    p2.font.name = "Arial"
    p2.font.size = Pt(22)
    p2.font.bold = True
    p2.font.color.rgb = ACCENT_COLOR
    p2.space_before = Pt(12)

    add_bullets(s9, [
        "Pode ser gerada aleatoriamente ou por heurística gulosa simples",
        "Deve obrigatoriamente respeitar a restrição de capacidade C (viável)",
        "Serve como ponto de partida da trajetória de busca da metaheurística",
        "Será iterativamente modificada durante a exploração do espaço de estados"
    ], top_inch=5.8, font_size=23, spacing=20)

    p_nota = s9.shapes.add_textbox(Inches(1.3), Inches(9.2), Inches(17.41), Inches(0.6))
    p = p_nota.text_frame.paragraphs[0]
    p.text = "* Valores e vetor acima são ilustrativos para demonstração conceitual da formulação."
    p.font.size = Pt(15)
    p.font.color.rgb = MUTED_COLOR

    add_footer(s9, "09")

    # =============================================================
    # SLIDE 10 — GERAÇÃO DE SUCESSOR
    # =============================================================
    s10 = prs.slides.add_slide(blank_layout)
    set_background(s10)
    add_header(s10, "10", "Vizinhança · Mutação de Bit")
    add_title(s10, "Como gerar uma nova solução?")

    # Diagrama de inversão de bit
    diag10 = s10.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(2.7))
    diag10.fill.solid()
    diag10.fill.fore_color.rgb = SURFACE_COLOR
    diag10.line.color.rgb = BORDER_COLOR
    diag10.line.width = Pt(1.5)
    tb10 = diag10.text_frame

    p = tb10.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "SOLUÇÃO ATUAL (S):       [  1 ,   0 ,   1 ,   0 ,   0 ,   1  ]"
    p.font.name = "Arial"
    p.font.size = Pt(28)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    p2 = tb10.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "↓  inverter uma posição aleatória (bit flip na posição 2: 0 → 1)"
    p2.font.name = "Arial"
    p2.font.size = Pt(20)
    p2.font.bold = True
    p2.font.color.rgb = ACCENT_COLOR
    p2.space_before = Pt(10)

    p3 = tb10.add_paragraph()
    p3.alignment = PP_ALIGN.CENTER
    p3.text = "NOVA SOLUÇÃO (S'):      [  1 ,   1 ,   1 ,   0 ,   0 ,   1  ]"
    p3.font.name = "Arial"
    p3.font.size = Pt(28)
    p3.font.bold = True
    p3.font.color.rgb = ACCENT_COLOR
    p3.space_before = Pt(10)

    add_bullets(s10, [
        "Um sucessor (solução vizinha) é gerado a partir da solução corrente",
        "Uma posição binária é sorteada e invertida (adiciona ou remove um item)",
        "O novo conjunto de itens é recalculado e avaliado",
        "Soluções inviáveis (peso > C) devem ser descartadas ou penalizadas pelo algoritmo"
    ], top_inch=6.3, font_size=23, spacing=18)

    add_footer(s10, "10")

    # =============================================================
    # SLIDE 11 — ANÁLISE DA SOLUÇÃO
    # =============================================================
    s11 = prs.slides.add_slide(blank_layout)
    set_background(s11)
    add_header(s11, "11", "Avaliação · Função Objetivo")
    add_title(s11, "Como avaliar uma solução?")

    # Dois cards comparativos
    c_p = s11.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(8.5), Inches(2.5))
    c_p.fill.solid()
    c_p.fill.fore_color.rgb = SURFACE_COLOR
    c_p.line.color.rgb = TEXT_COLOR
    c_p.line.width = Pt(2)
    p = c_p.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "RESTRIÇÃO DE VIABILIDADE\n\nPESO TOTAL  ≤  CAPACIDADE (C)"
    p.font.name = "Arial"
    p.font.size = Pt(24)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    c_v = s11.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(10.2), Inches(3.2), Inches(8.5), Inches(2.5))
    c_v.fill.solid()
    c_v.fill.fore_color.rgb = SURFACE_COLOR
    c_v.line.color.rgb = ACCENT_COLOR
    c_v.line.width = Pt(2)
    p = c_v.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "FUNÇÃO OBJETIVO\n\nVALOR TOTAL  =  ∑ (vᵢ · xᵢ)"
    p.font.name = "Arial"
    p.font.size = Pt(24)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    add_bullets(s11, [
        "Verificar a restrição de capacidade: se Peso(S) > C, a solução é inviável",
        "Calcular o valor total acumulado da combinação de itens selecionados",
        "Comparar o valor da nova solução com o valor da solução atual",
        "Identificar se houve melhoria ou piora para orientar a aceitação"
    ], top_inch=6.1, font_size=23, spacing=20)

    add_footer(s11, "11")

    # =============================================================
    # SLIDE 12 — CRITÉRIO DE ACEITAÇÃO
    # =============================================================
    s12 = prs.slides.add_slide(blank_layout)
    set_background(s12)
    add_header(s12, "12", "Critério de Metropolis · Maximização")
    add_title(s12, "Quando aceitar uma solução pior?")

    card12_d = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(8.5), Inches(1.8))
    card12_d.fill.solid()
    card12_d.fill.fore_color.rgb = SURFACE_COLOR
    card12_d.line.color.rgb = TEXT_COLOR
    card12_d.line.width = Pt(2)
    p = card12_d.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Δ  =  V(nova) − V(atual)"
    p.font.name = "Arial"
    p.font.size = Pt(32)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = card12_d.text_frame.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "Variação do valor da função objetivo (Maximização)"
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR

    card12_p = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(10.2), Inches(3.2), Inches(8.5), Inches(1.8))
    card12_p.fill.solid()
    card12_p.fill.fore_color.rgb = SURFACE_COLOR
    card12_p.line.color.rgb = ACCENT_COLOR
    card12_p.line.width = Pt(2)
    p = card12_p.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "P  =  e^( Δ / T )"
    p.font.name = "Arial"
    p.font.size = Pt(36)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR
    p2 = card12_p.text_frame.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "Probabilidade de aceitar piora (como Δ ≤ 0, P ≤ 1)"
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR

    # Duas caixas condicionais
    c_ok = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(5.3), Inches(8.5), Inches(1.6))
    c_ok.fill.solid()
    c_ok.fill.fore_color.rgb = SURFACE_COLOR
    c_ok.line.color.rgb = GREEN_ACCENT
    c_ok.line.width = Pt(2)
    p = c_ok.text_frame.paragraphs[0]
    p.text = "SE Δ > 0 (SOLUÇÃO MELHOR):"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = GREEN_ACCENT
    p2 = c_ok.text_frame.add_paragraph()
    p2.text = "Aceita imediatamente. O valor total da mochila aumentou."
    p2.font.size = Pt(17)
    p2.font.color.rgb = TEXT_COLOR

    c_bad = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(10.2), Inches(5.3), Inches(8.5), Inches(1.6))
    c_bad.fill.solid()
    c_bad.fill.fore_color.rgb = SURFACE_COLOR
    c_bad.line.color.rgb = ACCENT_COLOR
    c_bad.line.width = Pt(2)
    p = c_bad.text_frame.paragraphs[0]
    p.text = "SE Δ ≤ 0 (SOLUÇÃO PIOR):"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR
    p2 = c_bad.text_frame.add_paragraph()
    p2.text = "Pode aceitar com probabilidade P = exp(Δ / T). Se r < P, aceita."
    p2.font.size = Pt(17)
    p2.font.color.rgb = TEXT_COLOR

    add_bullets(s12, [
        "A temperatura T controla diretamente a tolerância a soluções piores",
        "Com o resfriamento progressivo (T diminui), a exploração diminui e o algoritmo converge"
    ], top_inch=7.3, font_size=22, spacing=16)

    add_footer(s12, "12")

    # =============================================================
    # SLIDE 13 — TEMPERATURA INICIAL
    # =============================================================
    s13 = prs.slides.add_slide(blank_layout)
    set_background(s13)
    add_header(s13, "13", "Parâmetro 01 · Inicialização")
    add_title(s13, "Temperatura inicial")

    # Display do parâmetro
    t_box13 = s13.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(1.8))
    t_box13.fill.solid()
    t_box13.fill.fore_color.rgb = SURFACE_COLOR
    t_box13.line.color.rgb = ACCENT_COLOR
    t_box13.line.width = Pt(2)
    p = t_box13.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "T  =  T₀"
    p.font.name = "Arial"
    p.font.size = Pt(44)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    add_bullets(s13, [
        "Define o ponto de partida do processo de resfriamento",
        "Temperatura alta permite maior probabilidade de aceitar soluções piores",
        "Garante ampla exploração global do espaço de busca no início",
        "Deve considerar a magnitude da função objetivo (escala dos valores vᵢ)",
        "Não existe valor universal: o valor definitivo deve ser calibrado experimentalmente"
    ], top_inch=5.4, font_size=23, spacing=20)

    add_footer(s13, "13")

    # =============================================================
    # SLIDE 14 — TEMPERATURA FINAL
    # =============================================================
    s14 = prs.slides.add_slide(blank_layout)
    set_background(s14)
    add_header(s14, "14", "Parâmetro 02 · Critério de Parada")
    add_title(s14, "Temperatura final")

    t_box14 = s14.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(1.8))
    t_box14.fill.solid()
    t_box14.fill.fore_color.rgb = SURFACE_COLOR
    t_box14.line.color.rgb = TEXT_COLOR
    t_box14.line.width = Pt(2)
    p = t_box14.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "T  →  Tmin"
    p.font.name = "Arial"
    p.font.size = Pt(44)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    add_bullets(s14, [
        "Define o critério formal de encerramento do processo de resfriamento",
        "Temperaturas próximas a Tmin reduzem drasticamente a exploração",
        "O algoritmo se comporta como uma busca local seletiva na fase final",
        "O valor ideal depende da tolerância computacional e da escala do problema",
        "Evita iterações desnecessárias quando a probabilidade P se torna insignificante"
    ], top_inch=5.4, font_size=23, spacing=20)

    add_footer(s14, "14")

    # =============================================================
    # SLIDE 15 — FATOR DE REDUÇÃO
    # =============================================================
    s15 = prs.slides.add_slide(blank_layout)
    set_background(s15)
    add_header(s15, "15", "Parâmetro 03 · Escalonamento")
    add_title(s15, "Fator de redução")

    t_box15 = s15.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(1.8))
    t_box15.fill.solid()
    t_box15.fill.fore_color.rgb = SURFACE_COLOR
    t_box15.line.color.rgb = ACCENT_COLOR
    t_box15.line.width = Pt(2)
    p = t_box15.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Tnovo  =  α · Tatual             |             α  ∈  ( 0 , 1 )"
    p.font.name = "Arial"
    p.font.size = Pt(36)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    add_bullets(s15, [
        "O parâmetro α (fator geométrico de resfriamento) controla a velocidade da queda térmica",
        "Valores de α próximos de 1 (ex: 0.95 a 0.99) resultam em resfriamento mais lento e robusto",
        "Valores menores de α aceleram o processo, mas aumentam o risco de convergência prematura",
        "O valor adequado deve ser determinado a partir de ensaios experimentais no Opcoda"
    ], top_inch=5.4, font_size=23, spacing=22)

    add_footer(s15, "15")

    # =============================================================
    # SLIDE 16 — ALGORITMO / PSEUDOCÓDIGO
    # =============================================================
    s16 = prs.slides.add_slide(blank_layout)
    set_background(s16)
    add_header(s16, "16", "Implementação · Lógica")
    add_title(s16, "Têmpera Simulada aplicada à Mochila")

    code_bg = s16.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(6.5))
    code_bg.fill.solid()
    code_bg.fill.fore_color.rgb = DARK_BOX
    code_bg.line.fill.background()

    code_bar = s16.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(0.2), Inches(6.5))
    code_bar.fill.solid()
    code_bar.fill.fore_color.rgb = ACCENT_COLOR
    code_bar.line.fill.background()

    tb16 = s16.shapes.add_textbox(Inches(1.8), Inches(3.4), Inches(16.5), Inches(6.1))
    tf16 = tb16.text_frame
    tf16.word_wrap = True

    pseudo = [
        "GERAR solução inicial S (viável: peso ≤ C)",
        "AVALIAR solução inicial: V(S)",
        "T = T₀",
        "ENQUANTO T > Tmin:",
        "    GERAR solução vizinha S' via mutação de bit",
        "    SE S' for viável (peso ≤ C):",
        "        Δ = V(S') − V(S)",
        "        SE Δ > 0:",
        "            S = S'   // aceita melhoria incondicional",
        "        SENÃO:",
        "            calcular probabilidade P = exp(Δ / T)",
        "            SE random(0, 1) < P:",
        "                S = S'   // aceita piora probabilisticamente",
        "    ATUALIZAR melhor solução encontrada",
        "    REDUZIR temperatura: T = α · T",
        "RETORNAR melhor solução"
    ]
    for i, line in enumerate(pseudo):
        p = tf16.paragraphs[0] if i == 0 else tf16.add_paragraph()
        p.text = line
        p.font.name = "Consolas"
        p.font.size = Pt(19)
        p.font.bold = any(k in line for k in ["GERAR", "ENQUANTO", "SE", "SENÃO", "RETORNAR"])
        if any(k in line for k in ["GERAR", "ENQUANTO", "SE", "SENÃO", "RETORNAR"]):
            p.font.color.rgb = ACCENT_COLOR
        elif "//" in line:
            p.font.color.rgb = MUTED_COLOR
        else:
            p.font.color.rgb = WHITE
        p.space_after = Pt(4)

    add_footer(s16, "16")

    # =============================================================
    # SLIDE 17 — FLUXOGRAMA
    # =============================================================
    s17 = prs.slides.add_slide(blank_layout)
    set_background(s17)
    add_header(s17, "17", "Fluxograma · Processo")
    add_title(s17, "Fluxo do algoritmo")

    # Caixa central estilizada ocupando a maior parte da tela
    fc_box = s17.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2), Inches(17.41), Inches(6.5))
    fc_box.fill.solid()
    fc_box.fill.fore_color.rgb = SURFACE_COLOR
    fc_box.line.color.rgb = BORDER_COLOR
    fc_box.line.width = Pt(1.5)

    # Nódulos do fluxograma
    f_nodes = [
        ("SOLUÇÃO INICIAL (S)", 1.7, 3.5, 3.8, 1.0, False),
        ("GERAR SUCESSOR (S')", 1.7, 4.8, 3.8, 1.0, False),
        ("AVALIAR (PESO ≤ C?)", 1.7, 6.1, 3.8, 1.0, False),
        ("MELHOROU? (Δ > 0)", 6.5, 4.8, 4.0, 1.0, True),
        ("ACEITAR (SIM)", 11.5, 3.8, 3.4, 0.9, False),
        ("PROBABILIDADE P = exp(Δ/T)", 11.5, 5.8, 4.2, 0.9, False),
        ("REDUZIR T = α · T", 11.5, 7.3, 3.4, 0.9, False),
        ("T > Tmin?", 7.0, 7.3, 3.0, 0.9, True),
        ("RESULTADO FINAL", 2.0, 7.3, 3.5, 0.9, False)
    ]

    for label, nx, ny, nw, nh, is_dec in f_nodes:
        bx = s17.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(nx), Inches(ny), Inches(nw), Inches(nh))
        bx.fill.solid()
        bx.fill.fore_color.rgb = HIGHLIGHT_BG if is_dec else (ACCENT_COLOR if "RESULTADO" in label else SURFACE_COLOR)
        bx.line.color.rgb = ACCENT_COLOR if is_dec else TEXT_COLOR
        bx.line.width = Pt(1.5)
        p = bx.text_frame.paragraphs[0]
        p.alignment = PP_ALIGN.CENTER
        p.text = label
        p.font.name = "Arial"
        p.font.size = Pt(16)
        p.font.bold = True
        p.font.color.rgb = WHITE if "RESULTADO" in label else (ACCENT_COLOR if is_dec else TEXT_COLOR)

    # Anotações de fluxo
    tb = s17.shapes.add_textbox(Inches(1.8), Inches(8.5), Inches(16.5), Inches(1.0))
    p = tb.text_frame.paragraphs[0]
    p.text = "Ciclo: Inicialização → Geração de vizinha → Teste de viabilidade e aceitação → Resfriamento → Critério de parada."
    p.font.size = Pt(17)
    p.font.bold = True
    p.font.color.rgb = MUTED_COLOR

    add_footer(s17, "17")

    # =============================================================
    # SLIDE 18 — APLICAÇÃO COMPLETA
    # =============================================================
    s18 = prs.slides.add_slide(blank_layout)
    set_background(s18)
    add_header(s18, "18", "Arquitetura · Pipeline Opcoda")
    add_title(s18, "Pipeline proposto")

    pipe_items = [
        ".EXE", "→", "BYTES", "→", "BLOCOS", "→", "ITENS", "→", "PESO+VALOR", "→", "MOCHILA", "→", "TÊMPERA SIMULADA", "→", "MELHOR COMBINAÇÃO", "→", "SÍNTESE GRANULAR"
    ]

    # Grid de blocos do pipeline
    steps18 = [
        ("1. ENTRADA", ".EXE → Bytes"),
        ("2. PARTICIONAMENTO", "Blocos → Itens"),
        ("3. MODELAGEM", "Peso + Valor"),
        ("4. MOCHILA", "Restrição C"),
        ("5. OTIMIZAÇÃO", "Têmpera Simulada"),
        ("6. SÍNTESE", "Áudio Granular")
    ]
    for i, (t_st, d_st) in enumerate(steps18):
        is_opt = ("OTIMIZAÇÃO" in t_st)
        bx = s18.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3 + i * 2.95), Inches(3.2), Inches(2.7), Inches(1.8))
        bx.fill.solid()
        bx.fill.fore_color.rgb = HIGHLIGHT_BG if is_opt else SURFACE_COLOR
        bx.line.color.rgb = ACCENT_COLOR if is_opt else BORDER_COLOR
        bx.line.width = Pt(2.5 if is_opt else 1.5)
        p = bx.text_frame.paragraphs[0]
        p.text = t_st
        p.font.name = "Arial"
        p.font.size = Pt(15)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR if is_opt else MUTED_COLOR
        p2 = bx.text_frame.add_paragraph()
        p2.text = d_st
        p2.font.name = "Arial"
        p2.font.size = Pt(19)
        p2.font.bold = True
        p2.font.color.rgb = ACCENT_COLOR if is_opt else TEXT_COLOR
        p2.space_before = Pt(8)

    add_bullets(s18, [
        "A Têmpera Simulada atua como filtro seletivo ótimo antes da síntese granular",
        "Apenas os blocos binários selecionados na melhor combinação alimentarão os grãos",
        "O processamento ocorre de forma desacoplada para preservar a execução de áudio em tempo real"
    ], top_inch=5.7, font_size=23, spacing=24)

    add_footer(s18, "18")

    # =============================================================
    # SLIDE 19 — PARÂMETROS A IDENTIFICAR
    # =============================================================
    s19 = prs.slides.add_slide(blank_layout)
    set_background(s19)
    add_header(s19, "19", "Requisitos · Parâmetros da Atividade")
    add_title(s19, "Parâmetros do experimento")

    cards19 = [
        ("01", "ALGORITMO / FLUXO", "Pseudocódigo estruturado e fluxograma de decisão implementados"),
        ("02", "TEMPERATURA INICIAL", "T₀ define a taxa inicial de exploração de soluções viáveis"),
        ("03", "TEMPERATURA FINAL", "Tmin estabelece o critério de parada e congelamento da busca"),
        ("04", "FATOR DE REDUÇÃO", "α define a taxa de escalonamento geométrico: T = α · T")
    ]
    for i, (num_st, tit_st, desc_st) in enumerate(cards19):
        bx = s19.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3 + i * 4.45), Inches(3.2), Inches(4.1), Inches(3.8))
        bx.fill.solid()
        bx.fill.fore_color.rgb = SURFACE_COLOR
        bx.line.color.rgb = BORDER_COLOR
        bx.line.width = Pt(1.5)

        tb = bx.text_frame
        tb.word_wrap = True
        p = tb.paragraphs[0]
        p.text = num_st
        p.font.name = "Arial"
        p.font.size = Pt(32)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR

        p2 = tb.add_paragraph()
        p2.text = tit_st
        p2.font.name = "Arial"
        p2.font.size = Pt(18)
        p2.font.bold = True
        p2.font.color.rgb = TEXT_COLOR
        p2.space_before = Pt(8)

        p3 = tb.add_paragraph()
        p3.text = desc_st
        p3.font.name = "Arial"
        p3.font.size = Pt(16)
        p3.font.color.rgb = MUTED_COLOR
        p3.space_before = Pt(12)

    c19 = s19.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(7.5), Inches(17.41), Inches(1.8))
    c19.fill.solid()
    c19.fill.fore_color.rgb = HIGHLIGHT_BG
    c19.line.color.rgb = ACCENT_COLOR
    c19.line.width = Pt(2)
    p = c19.text_frame.paragraphs[0]
    p.text = "“Os valores definitivos de T₀, Tmin e α devem ser definidos e justificados a partir de testes empíricos.”"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = c19.text_frame.add_paragraph()
    p2.text = "A metodologia prevê baterias de testes com diferentes configurações para encontrar a convergência ótima."
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(6)

    add_footer(s19, "19")

    # =============================================================
    # SLIDE 20 — AVALIAÇÃO
    # =============================================================
    s20 = prs.slides.add_slide(blank_layout)
    set_background(s20)
    add_header(s20, "20", "Metodologia Experimental")
    add_title(s20, "Como avaliar o resultado?")
    add_bullets(s20, [
        "Valor da solução inicial vs. valor da melhor solução encontrada",
        "Percentual da capacidade da mochila efetivamente utilizado (aproveitamento de C)",
        "Taxa de convergência da função objetivo em relação ao número de iterações",
        "Sensibilidade do algoritmo frente a variações do fator de redução α",
        "Tempo de execução computacional para diferentes tamanhos de arquivo executável"
    ], top_inch=3.4, font_size=23, spacing=20)

    c20 = s20.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(7.8), Inches(17.41), Inches(1.5))
    c20.fill.solid()
    c20.fill.fore_color.rgb = SURFACE_COLOR
    c20.line.color.rgb = BORDER_COLOR
    c20.line.width = Pt(1.5)
    p = c20.text_frame.paragraphs[0]
    p.text = "Etapa experimental a ser realizada nos próximos ciclos de desenvolvimento."
    p.font.name = "Arial"
    p.font.size = Pt(21)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = c20.text_frame.add_paragraph()
    p2.text = "Apresenta-se a proposta de modelagem matemática e algorítmica; os dados numéricos serão medidos em bancada."
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(6)

    add_footer(s20, "20")

    # =============================================================
    # SLIDE 21 — CONTRIBUIÇÃO PARA O OPCODA
    # =============================================================
    s21 = prs.slides.add_slide(blank_layout)
    set_background(s21)
    add_header(s21, "21", "Impacto · Contribuição Acadêmica")
    add_title(s21, "O que a otimização acrescenta?")
    add_bullets(s21, [
        "Introduz otimização combinatória clássica à arquitetura de síntese sonora",
        "Permite selecionar combinações ótimas de blocos sob restrição real de capacidade",
        "Conecta os conceitos da disciplina de Programação Linear à computação musical",
        "Cria uma metodologia experimental estruturada para curadoria automática de dados binários"
    ], top_inch=3.5, font_size=24, spacing=26)

    c21 = s21.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(7.8), Inches(17.41), Inches(1.4))
    c21.fill.solid()
    c21.fill.fore_color.rgb = HIGHLIGHT_BG
    c21.line.color.rgb = ACCENT_COLOR
    c21.line.width = Pt(2)
    p = c21.text_frame.paragraphs[0]
    p.text = "A abordagem estabelece um critério formal de seleção sem pressupor benefícios sonoros a priori."
    p.font.name = "Arial"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = c21.text_frame.add_paragraph()
    p2.text = "Qualquer avaliação estética ou acústica dependerá de ensaios e escutas subsequentes."
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(6)

    add_footer(s21, "21")

    # =============================================================
    # SLIDE 22 — REFERÊNCIAS
    # =============================================================
    s22 = prs.slides.add_slide(blank_layout)
    set_background(s22)
    add_header(s22, "22", "Fundamentação Bibliográfica")
    add_title(s22, "Referências")

    refs22 = [
        ("KIRKPATRICK, S.; GELATT, C. D.; VECCHI, M. P.", "Optimization by Simulated Annealing. Science, v. 220, n. 4598, p. 671–680, 1983."),
        ("AARTS, E. H. L.; VAN LAARHOVEN, P. J. M.", "Simulated Annealing: Theory and Applications. Dordrecht: Kluwer Academic Publishers, 1989."),
        ("TRIKI, E.; COLLETTE, Y.; SIARRY, P.", "A Theoretical Study on the Behavior of Simulated Annealing Leading to a New Cooling Schedule. European Journal of Operational Research, 2005."),
        ("ROADS, Curtis.", "Microsound. Cambridge: The MIT Press, 2004.")
    ]
    for i, (autor, obra) in enumerate(refs22):
        bx = s22.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.3), Inches(3.2 + i * 1.55), Inches(17.41), Inches(1.3))
        bx.fill.solid()
        bx.fill.fore_color.rgb = SURFACE_COLOR
        bx.line.color.rgb = BORDER_COLOR
        bx.line.width = Pt(1.5)
        tb = bx.text_frame
        p = tb.paragraphs[0]
        p.text = autor
        p.font.name = "Arial"
        p.font.size = Pt(18)
        p.font.bold = True
        p.font.color.rgb = TEXT_COLOR
        p2 = tb.add_paragraph()
        p2.text = obra
        p2.font.name = "Arial"
        p2.font.size = Pt(16)
        p2.font.color.rgb = MUTED_COLOR
        p2.space_before = Pt(4)

    add_footer(s22, "22")

    # =============================================================
    # SLIDE 23 — ENCERRAMENTO
    # =============================================================
    s23 = prs.slides.add_slide(blank_layout)
    set_background(s23)
    add_header(s23, "23", "Conclusão · Agradecimentos")

    t23 = s23.shapes.add_textbox(Inches(1.3), Inches(2.3), Inches(17.41), Inches(1.2))
    p = t23.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Opcoda + Têmpera Simulada"
    p.font.name = "Arial"
    p.font.size = Pt(46)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    # Equação visual
    eq_items23 = [
        ("DADOS BINÁRIOS", False),
        ("+", None),
        ("PROBLEMA DA MOCHILA", False),
        ("+", None),
        ("TÊMPERA SIMULADA", False),
        ("+", None),
        ("SÍNTESE GRANULAR", False),
        ("=", None),
        ("OPCODA", True)
    ]
    x = 1.3
    for label, is_acc in eq_items23:
        if is_acc is None:
            tb = s23.shapes.add_textbox(Inches(x), Inches(4.3), Inches(0.6), Inches(1.0))
            p = tb.text_frame.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER
            p.text = label
            p.font.name = "Arial"
            p.font.size = Pt(28)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR
            x += 0.65
        else:
            w_box = 2.8 if "MOCHILA" in label or "TÊMPERA" in label else (2.4 if "DADOS" in label or "SÍNTESE" in label else 2.0)
            bx = s23.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(4.2), Inches(w_box), Inches(1.2))
            bx.fill.solid()
            bx.fill.fore_color.rgb = ACCENT_COLOR if is_acc else SURFACE_COLOR
            bx.line.color.rgb = ACCENT_COLOR if is_acc else TEXT_COLOR
            bx.line.width = Pt(1.5)
            p = bx.text_frame.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER
            p.text = label
            p.font.name = "Arial"
            p.font.size = Pt(15)
            p.font.bold = True
            p.font.color.rgb = WHITE if is_acc else TEXT_COLOR
            x += w_box + 0.15

    # Card final de autores
    c23 = s23.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(4.5), Inches(6.5), Inches(11.0), Inches(2.2))
    c23.fill.solid()
    c23.fill.fore_color.rgb = SURFACE_COLOR
    c23.line.color.rgb = TEXT_COLOR
    c23.line.width = Pt(2)
    tb23 = c23.text_frame
    p = tb23.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Mateus Costa · Rafael Elisei"
    p.font.name = "Arial"
    p.font.size = Pt(26)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb23.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "5º semestre · Análise e Desenvolvimento de Sistemas · FATEC Cruzeiro\nDisciplina de Programação Linear / Otimização"
    p2.font.name = "Arial"
    p2.font.size = Pt(18)
    p2.font.color.rgb = MUTED_COLOR
    p2.space_before = Pt(8)

    add_footer(s23, "23")

    prs.save(output_path)
    print(f"Apresentação da Mochila gerada com sucesso em: {output_path}")

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "docs/apresentacao-mochila-tempera-simulada.pptx"
    create_mochila_deck(out)
