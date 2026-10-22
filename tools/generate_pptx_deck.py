import sys
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN
from pptx.enum.shapes import MSO_SHAPE

def create_deck(output_path):
    prs = Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5)

    # Colors
    BG_COLOR = RGBColor(0xF4, 0xF5, 0xF6)       # Off-white
    SURFACE_COLOR = RGBColor(0xFF, 0xFF, 0xFF)  # White card
    TEXT_COLOR = RGBColor(0x12, 0x15, 0x18)     # Near Black
    MUTED_COLOR = RGBColor(0x5A, 0x60, 0x65)    # Gray
    ACCENT_COLOR = RGBColor(0xFF, 0x9A, 0x00)   # Opcoda Amber
    BORDER_COLOR = RGBColor(0xD8, 0xDC, 0xDE)   # Light gray border
    DARK_BOX = RGBColor(0x0E, 0x10, 0x13)       # Terminal Dark
    WHITE = RGBColor(0xFF, 0xFF, 0xFF)

    blank_layout = prs.slide_layouts[6]

    def set_slide_background(slide):
        bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, prs.slide_width, prs.slide_height)
        bg.fill.solid()
        bg.fill.fore_color.rgb = BG_COLOR
        bg.line.fill.background()
        return bg

    def add_header(slide, number_str, section_str, speaker_str=""):
        # Slide number
        num_box = slide.shapes.add_textbox(Inches(0.8), Inches(0.5), Inches(1.2), Inches(0.6))
        tf = num_box.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        p.text = number_str
        p.font.name = "Arial"
        p.font.size = Pt(20)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR

        # Section tag
        sec_box = slide.shapes.add_textbox(Inches(2.0), Inches(0.5), Inches(7.5), Inches(0.6))
        tf = sec_box.text_frame
        p = tf.paragraphs[0]
        p.text = section_str.upper()
        p.font.name = "Arial"
        p.font.size = Pt(13)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

        # Divider line
        line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(1.15), Inches(11.733), Inches(0.02))
        line.fill.solid()
        line.fill.fore_color.rgb = TEXT_COLOR
        line.line.fill.background()

    def add_footer(slide, number_str):
        # Footer line
        line = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(6.8), Inches(11.733), Inches(0.015))
        line.fill.solid()
        line.fill.fore_color.rgb = BORDER_COLOR
        line.line.fill.background()

        # Left label
        fb = slide.shapes.add_textbox(Inches(0.8), Inches(6.85), Inches(6.0), Inches(0.4))
        p = fb.text_frame.paragraphs[0]
        p.text = "OPCODA · OTIMIZAÇÃO POR TÊMPERA SIMULADA"
        p.font.name = "Arial"
        p.font.size = Pt(11)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

        # Right count
        fb_r = slide.shapes.add_textbox(Inches(10.533), Inches(6.85), Inches(2.0), Inches(0.4))
        p = fb_r.text_frame.paragraphs[0]
        p.alignment = PP_ALIGN.RIGHT
        p.text = f"{number_str} / 17"
        p.font.name = "Arial"
        p.font.size = Pt(11)
        p.font.bold = True
        p.font.color.rgb = MUTED_COLOR

    def add_title(slide, title_text):
        tb = slide.shapes.add_textbox(Inches(0.8), Inches(1.35), Inches(11.733), Inches(1.0))
        tf = tb.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        p.text = title_text
        p.font.name = "Arial"
        p.font.size = Pt(36)
        p.font.bold = True
        p.font.color.rgb = TEXT_COLOR

    def add_bullets(slide, items, top_inch=2.6, left_inch=0.8, width_inch=11.733, font_size=24):
        tb = slide.shapes.add_textbox(Inches(left_inch), Inches(top_inch), Inches(width_inch), Inches(3.8))
        tf = tb.text_frame
        tf.word_wrap = True
        for i, text in enumerate(items):
            p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
            p.text = f"■   {text}"
            p.font.name = "Arial"
            p.font.size = Pt(font_size)
            p.font.bold = False
            p.font.color.rgb = TEXT_COLOR
            p.space_after = Pt(20)

    # -------------------------------------------------------------
    # SLIDE 1 — CAPA
    # -------------------------------------------------------------
    s1 = prs.slides.add_slide(blank_layout)
    set_slide_background(s1)
    add_header(s1, "01", "FATEC Cruzeiro · Programação Linear")

    tag = s1.shapes.add_textbox(Inches(0.8), Inches(1.8), Inches(10), Inches(0.5))
    tag.text_frame.paragraphs[0].text = "TRABALHO DE OTIMIZAÇÃO · PROJETO OPCODA"
    tag.text_frame.paragraphs[0].font.name = "Arial"
    tag.text_frame.paragraphs[0].font.size = Pt(15)
    tag.text_frame.paragraphs[0].font.bold = True
    tag.text_frame.paragraphs[0].font.color.rgb = ACCENT_COLOR

    t1 = s1.shapes.add_textbox(Inches(0.8), Inches(2.3), Inches(11.7), Inches(2.0))
    tf1 = t1.text_frame
    tf1.word_wrap = True
    p1 = tf1.paragraphs[0]
    p1.text = "Aplicação de Têmpera Simulada no Opcoda"
    p1.font.name = "Arial"
    p1.font.size = Pt(44)
    p1.font.bold = True
    p1.font.color.rgb = TEXT_COLOR

    sub1 = s1.shapes.add_textbox(Inches(0.8), Inches(3.4), Inches(11.7), Inches(1.0))
    p_sub = sub1.text_frame.paragraphs[0]
    p_sub.text = "Otimização da ordenação de grãos sonoros derivados de arquivos executáveis"
    p_sub.font.name = "Arial"
    p_sub.font.size = Pt(24)
    p_sub.font.color.rgb = MUTED_COLOR

    # Card autores
    card = s1.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(4.7), Inches(7.5), Inches(1.5))
    card.fill.solid()
    card.fill.fore_color.rgb = SURFACE_COLOR
    card.line.color.rgb = BORDER_COLOR
    card.line.width = Pt(1)

    bar = s1.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(4.7), Inches(0.12), Inches(1.5))
    bar.fill.solid()
    bar.fill.fore_color.rgb = ACCENT_COLOR
    bar.line.fill.background()

    aut = s1.shapes.add_textbox(Inches(1.1), Inches(4.85), Inches(7.0), Inches(1.2))
    p_aut = aut.text_frame.paragraphs[0]
    p_aut.text = "Mateus Costa · Rafael Elisei"
    p_aut.font.name = "Arial"
    p_aut.font.size = Pt(22)
    p_aut.font.bold = True
    p_aut.font.color.rgb = TEXT_COLOR
    p_aut2 = aut.text_frame.add_paragraph()
    p_aut2.text = "5º semestre · Análise e Desenvolvimento de Sistemas\nFATEC Cruzeiro"
    p_aut2.font.name = "Arial"
    p_aut2.font.size = Pt(16)
    p_aut2.font.color.rgb = MUTED_COLOR
    add_footer(s1, "01")

    # -------------------------------------------------------------
    # SLIDE 2 — O OPCODA
    # -------------------------------------------------------------
    s2 = prs.slides.add_slide(blank_layout)
    set_slide_background(s2)
    add_header(s2, "02", "Fundamentação · Contexto")
    add_title(s2, "O que é o Opcoda?")
    add_bullets(s2, [
        "Instrumento virtual experimental em tempo real",
        "Arquivos executáveis (.exe) utilizados como matéria-prima",
        "Dados binários convertidos diretamente em material sonoro",
        "Síntese granular empregada para geração sonora"
    ], top_inch=2.5, font_size=23)

    # Pipeline visual
    nodes = ["ARQUIVO .EXE", "→", "DADOS", "→", "BLOCOS", "→", "GRÃOS", "→", "SOM"]
    x = 0.8
    for item in nodes:
        if item == "→":
            tb = s2.shapes.add_textbox(Inches(x), Inches(5.6), Inches(0.5), Inches(0.7))
            p = tb.text_frame.paragraphs[0]
            p.text = "→"
            p.font.name = "Arial"
            p.font.size = Pt(24)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR
            x += 0.5
        else:
            is_accent = (item == "SOM")
            box = s2.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(5.5), Inches(2.0 if item == "ARQUIVO .EXE" else 1.6), Inches(0.8))
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
            p.font.color.rgb = TEXT_COLOR if is_accent else TEXT_COLOR
            x += (2.1 if item == "ARQUIVO .EXE" else 1.7)

    add_footer(s2, "02")

    # -------------------------------------------------------------
    # SLIDE 3 — O PROBLEMA
    # -------------------------------------------------------------
    s3 = prs.slides.add_slide(blank_layout)
    set_slide_background(s3)
    add_header(s3, "03", "Problema · Motivação")
    add_title(s3, "Onde entra a otimização?")
    add_bullets(s3, [
        "Um arquivo pode gerar centenas de grãos sonoros",
        "A ordem dos grãos altera diretamente as transições",
        "Precisamos avaliar diferentes sequências de reprodução",
        "O objetivo formal é minimizar uma função de custo global"
    ], top_inch=2.5, left_inch=0.8, width_inch=6.5, font_size=21)

    # Box Sequências à direita
    card_seq1 = s3.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(7.6), Inches(2.5), Inches(4.9), Inches(1.8))
    card_seq1.fill.solid()
    card_seq1.fill.fore_color.rgb = SURFACE_COLOR
    card_seq1.line.color.rgb = BORDER_COLOR
    tb = s3.shapes.add_textbox(Inches(7.8), Inches(2.6), Inches(4.5), Inches(1.5))
    tb.text_frame.word_wrap = True
    p = tb.text_frame.paragraphs[0]
    p.text = "SEQUÊNCIA INICIAL\nA  →  B  →  C  →  D  →  E"
    p.font.name = "Arial"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb.text_frame.add_paragraph()
    p2.text = "Custo acumulado elevado (transições bruscas)"
    p2.font.size = Pt(14)
    p2.font.color.rgb = MUTED_COLOR

    card_seq2 = s3.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(7.6), Inches(4.6), Inches(4.9), Inches(1.8))
    card_seq2.fill.solid()
    card_seq2.fill.fore_color.rgb = RGBColor(0xFF, 0xFD, 0xF9)
    card_seq2.line.color.rgb = ACCENT_COLOR
    card_seq2.line.width = Pt(2)
    tb2 = s3.shapes.add_textbox(Inches(7.8), Inches(4.7), Inches(4.5), Inches(1.5))
    tb2.text_frame.word_wrap = True
    p = tb2.text_frame.paragraphs[0]
    p.text = "SEQUÊNCIA REORDENADA\nA  →  D  →  B  →  E  →  C"
    p.font.name = "Arial"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR
    p2 = tb2.text_frame.add_paragraph()
    p2.text = "Custo minimizado pelo algoritmo de otimização"
    p2.font.size = Pt(14)
    p2.font.color.rgb = TEXT_COLOR
    add_footer(s3, "03")

    # -------------------------------------------------------------
    # SLIDE 4 — RELAÇÃO COM O TSP
    # -------------------------------------------------------------
    s4 = prs.slides.add_slide(blank_layout)
    set_slide_background(s4)
    add_header(s4, "04", "Modelagem · Analogia")
    add_title(s4, "Uma abordagem semelhante ao Caixeiro Viajante")

    # Tabela comparativa
    table_shape = s4.shapes.add_table(5, 2, Inches(0.8), Inches(2.4), Inches(11.733), Inches(2.6))
    table = table_shape.table
    table.columns[0].width = Inches(5.866)
    table.columns[1].width = Inches(5.866)

    headers = ["Problema do Caixeiro Viajante (TSP)", "Ordenação de Grãos no Opcoda"]
    data = [
        ("Cidade", "Grão Sonoro"),
        ("Distância entre cidades", "Diferença entre grãos consecutivas"),
        ("Rota percorrida", "Sequência temporal de reprodução"),
        ("Custo total da rota", "Custo total acumulado das transições")
    ]
    for col_idx, h in enumerate(headers):
        cell = table.cell(0, col_idx)
        cell.fill.solid()
        cell.fill.fore_color.rgb = TEXT_COLOR
        p = cell.text_frame.paragraphs[0]
        p.text = h
        p.font.name = "Arial"
        p.font.size = Pt(16)
        p.font.bold = True
        p.font.color.rgb = WHITE

    for row_idx, row in enumerate(data):
        for col_idx, val in enumerate(row):
            cell = table.cell(row_idx + 1, col_idx)
            cell.fill.solid()
            cell.fill.fore_color.rgb = SURFACE_COLOR
            p = cell.text_frame.paragraphs[0]
            p.text = val
            p.font.name = "Arial"
            p.font.size = Pt(18)
            p.font.bold = (col_idx == 1)
            p.font.color.rgb = ACCENT_COLOR if col_idx == 1 else TEXT_COLOR

    # Callout
    c_box = s4.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.3), Inches(11.733), Inches(1.1))
    c_box.fill.solid()
    c_box.fill.fore_color.rgb = SURFACE_COLOR
    c_box.line.color.rgb = ACCENT_COLOR
    c_box.line.width = Pt(2)
    p = c_box.text_frame.paragraphs[0]
    p.text = "“O problema pode ser tratado formalmente como uma otimização de permutação.”"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    add_footer(s4, "04")

    # -------------------------------------------------------------
    # SLIDE 5 — TÊMPERA SIMULADA
    # -------------------------------------------------------------
    s5 = prs.slides.add_slide(blank_layout)
    set_slide_background(s5)
    add_header(s5, "05", "Algoritmo · Metaheurística")
    add_title(s5, "O que é Têmpera Simulada?")
    add_bullets(s5, [
        "Metaheurística de otimização combinatória",
        "Inspirada no processo físico de resfriamento e recozimento de metais",
        "Explora o espaço de soluções aceitando movimentos piores temporariamente",
        "Reduz progressivamente a temperatura ao longo das iterações"
    ], top_inch=2.4, font_size=23)

    c5 = s5.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.1), Inches(11.733), Inches(1.3))
    c5.fill.solid()
    c5.fill.fore_color.rgb = SURFACE_COLOR
    c5.line.color.rgb = BORDER_COLOR
    b5 = s5.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.1), Inches(0.12), Inches(1.3))
    b5.fill.solid()
    b5.fill.fore_color.rgb = ACCENT_COLOR
    b5.line.fill.background()
    tb5 = s5.shapes.add_textbox(Inches(1.1), Inches(5.2), Inches(11.0), Inches(1.1))
    p = tb5.text_frame.paragraphs[0]
    p.text = "“Permitir movimentos piores ajuda a escapar de mínimos locais.”"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb5.text_frame.add_paragraph()
    p2.text = "Referência: Kirkpatrick, Gelatt e Vecchi (1983)"
    p2.font.size = Pt(15)
    p2.font.color.rgb = MUTED_COLOR
    add_footer(s5, "05")

    # -------------------------------------------------------------
    # SLIDE 6 — COMO O ALGORITMO FUNCIONA
    # -------------------------------------------------------------
    s6 = prs.slides.add_slide(blank_layout)
    set_slide_background(s6)
    add_header(s6, "06", "Fluxo de Execução")
    add_title(s6, "Funcionamento")

    steps = ["Solução Inicial", "→", "Gerar Vizinha", "→", "Calcular Custo", "→", "Melhorou?", "→", "Reduzir T", "→", "Repetir"]
    x = 0.8
    for st in steps:
        if st == "→":
            tb = s6.shapes.add_textbox(Inches(x), Inches(2.6), Inches(0.4), Inches(0.7))
            p = tb.text_frame.paragraphs[0]
            p.text = "→"
            p.font.name = "Arial"
            p.font.size = Pt(22)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR
            x += 0.45
        else:
            is_dec = (st == "Melhorou?")
            box = s6.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(2.5), Inches(1.5), Inches(0.9))
            box.fill.solid()
            box.fill.fore_color.rgb = DARK_BOX if is_dec else SURFACE_COLOR
            box.line.color.rgb = DARK_BOX
            box.line.width = Pt(1.5)
            p = box.text_frame.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER
            p.text = st
            p.font.name = "Arial"
            p.font.size = Pt(16)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR if is_dec else TEXT_COLOR
            x += 1.55

    # Duas caixas de decisão
    c_yes = s6.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(4.0), Inches(5.6), Inches(2.2))
    c_yes.fill.solid()
    c_yes.fill.fore_color.rgb = SURFACE_COLOR
    c_yes.line.color.rgb = RGBColor(0x4C, 0xAF, 0x50)
    c_yes.line.width = Pt(2)
    tb = s6.shapes.add_textbox(Inches(1.0), Inches(4.2), Inches(5.2), Inches(1.8))
    tb.text_frame.word_wrap = True
    p = tb.text_frame.paragraphs[0]
    p.text = "SE MELHOROU (Δ ≤ 0):"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = RGBColor(0x2E, 0x7D, 0x32)
    p2 = tb.text_frame.add_paragraph()
    p2.text = "A nova solução é aceita imediatamente como a solução corrente."
    p2.font.size = Pt(18)
    p2.font.color.rgb = TEXT_COLOR

    c_no = s6.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(6.9), Inches(4.0), Inches(5.6), Inches(2.2))
    c_no.fill.solid()
    c_no.fill.fore_color.rgb = SURFACE_COLOR
    c_no.line.color.rgb = ACCENT_COLOR
    c_no.line.width = Pt(2)
    tb2 = s6.shapes.add_textbox(Inches(7.1), Inches(4.2), Inches(5.2), Inches(1.8))
    tb2.text_frame.word_wrap = True
    p = tb2.text_frame.paragraphs[0]
    p.text = "SE PIOROU (Δ > 0):"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR
    p2 = tb2.text_frame.add_paragraph()
    p2.text = "Calcula-se a probabilidade P = exp(-Δ/T). Se random(0,1) < P, a solução é aceita."
    p2.font.size = Pt(18)
    p2.font.color.rgb = TEXT_COLOR

    add_footer(s6, "06")

    # -------------------------------------------------------------
    # SLIDE 7 — REPRESENTAÇÃO DA SOLUÇÃO
    # -------------------------------------------------------------
    s7 = prs.slides.add_slide(blank_layout)
    set_slide_background(s7)
    add_header(s7, "07", "Modelagem · Estrutura")
    add_title(s7, "Como representamos uma solução?")

    card_s = s7.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.4), Inches(11.733), Inches(1.2))
    card_s.fill.solid()
    card_s.fill.fore_color.rgb = SURFACE_COLOR
    card_s.line.color.rgb = TEXT_COLOR
    card_s.line.width = Pt(2)
    p = card_s.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "S  =  [  G₁ ,  G₂ ,  G₃ ,  G₄ ,  G₅  ]"
    p.font.name = "Arial"
    p.font.size = Pt(36)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    add_bullets(s7, [
        "Cada Gᵢ representa um grão sonoro individual",
        "A solução formal é uma permutação dos índices dos grãos",
        "A ordem temporal do vetor define a sequência de reprodução",
        "O algoritmo busca a permutação que minimize o custo total"
    ], top_inch=4.0, font_size=23)
    add_footer(s7, "07")

    # -------------------------------------------------------------
    # SLIDE 8 — FUNÇÃO DE CUSTO
    # -------------------------------------------------------------
    s8 = prs.slides.add_slide(blank_layout)
    set_slide_background(s8)
    add_header(s8, "08", "Função Objetivo")
    add_title(s8, "Como avaliar uma sequência?")

    f_box = s8.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3), Inches(6.0), Inches(1.3))
    f_box.fill.solid()
    f_box.fill.fore_color.rgb = SURFACE_COLOR
    f_box.line.color.rgb = TEXT_COLOR
    f_box.line.width = Pt(2)
    p = f_box.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "C(S) = ∑ d(Gᵢ , Gᵢ₊₁)"
    p.font.name = "Arial"
    p.font.size = Pt(32)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    t_box = s8.shapes.add_textbox(Inches(7.2), Inches(2.4), Inches(5.3), Inches(1.1))
    p = t_box.text_frame.paragraphs[0]
    p.text = "d(Gᵢ, Gᵢ₊₁)  =\ndiferença entre dois grãos consecutivos"
    p.font.name = "Arial"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = MUTED_COLOR

    add_bullets(s8, [
        "Cada transição de reprodução possui um custo associado",
        "Os custos de transição são acumulados linearmente na sequência",
        "O objetivo da têmpera simulada é minimizar C(S)"
    ], top_inch=4.0, font_size=23)

    c8 = s8.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.7), Inches(11.733), Inches(0.8))
    c8.fill.solid()
    c8.fill.fore_color.rgb = SURFACE_COLOR
    c8.line.color.rgb = BORDER_COLOR
    p = c8.text_frame.paragraphs[0]
    p.text = "A métrica d(Gᵢ, Gᵢ₊₁) é tratada de forma abstrata neste estágio e será calibrada experimentalmente."
    p.font.size = Pt(16)
    p.font.color.rgb = MUTED_COLOR
    add_footer(s8, "08")

    # -------------------------------------------------------------
    # SLIDE 9 — MOVIMENTO 2-OPT
    # -------------------------------------------------------------
    s9 = prs.slides.add_slide(blank_layout)
    set_slide_background(s9)
    add_header(s9, "09", "Vizinhança · Mutação")
    add_title(s9, "Como gerar uma nova solução?")

    opt_box = s9.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3), Inches(11.733), Inches(1.8))
    opt_box.fill.solid()
    opt_box.fill.fore_color.rgb = SURFACE_COLOR
    opt_box.line.color.rgb = BORDER_COLOR
    tb = s9.shapes.add_textbox(Inches(1.0), Inches(2.4), Inches(11.3), Inches(1.6))
    p = tb.text_frame.paragraphs[0]
    p.text = "ATUAL:       A  →  B  →  [ C  →  D  →  E ]  →  F"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb.text_frame.add_paragraph()
    p2.text = "                                       ↓ Inversão do segmento 2-opt"
    p2.font.size = Pt(18)
    p2.font.bold = True
    p2.font.color.rgb = ACCENT_COLOR
    p3 = tb.text_frame.add_paragraph()
    p3.text = "VIZINHA:    A  →  B  →  [ E  →  D  →  C ]  →  F"
    p3.font.size = Pt(22)
    p3.font.bold = True
    p3.font.color.rgb = ACCENT_COLOR

    add_bullets(s9, [
        "Dois pontos de corte da sequência são selecionados aleatoriamente",
        "O segmento intermediário é invertido gerando uma nova permutação",
        "Uma nova sequência válida é criada sem repetições de elementos",
        "2-opt é a estratégia de vizinhança padrão em problemas de roteamento/TSP"
    ], top_inch=4.4, font_size=21)
    add_footer(s9, "09")

    # -------------------------------------------------------------
    # SLIDE 10 — CRITÉRIO DE ACEITAÇÃO
    # -------------------------------------------------------------
    s10 = prs.slides.add_slide(blank_layout)
    set_slide_background(s10)
    add_header(s10, "10", "Critério de Metropolis")
    add_title(s10, "E se a nova solução for pior?")

    card_d = s10.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3), Inches(5.6), Inches(1.2))
    card_d.fill.solid()
    card_d.fill.fore_color.rgb = SURFACE_COLOR
    card_d.line.color.rgb = TEXT_COLOR
    card_d.line.width = Pt(2)
    p = card_d.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Δ = C(nova) − C(atual)"
    p.font.name = "Arial"
    p.font.size = Pt(28)
    p.font.bold = True

    card_p = s10.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(6.9), Inches(2.3), Inches(5.6), Inches(1.2))
    card_p.fill.solid()
    card_p.fill.fore_color.rgb = SURFACE_COLOR
    card_p.line.color.rgb = ACCENT_COLOR
    card_p.line.width = Pt(2)
    p = card_p.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "P = e^(−Δ / T)"
    p.font.name = "Arial"
    p.font.size = Pt(28)
    p.font.bold = True
    p.font.color.rgb = ACCENT_COLOR

    add_bullets(s10, [
        "Se Δ ≤ 0 → Nova solução é melhor: aceita incondicionalmente",
        "Se Δ > 0 → Nova solução é pior: pode ser aceita com probabilidade P",
        "A probabilidade de aceitar pioras depende de Δ e da temperatura T",
        "Conforme o sistema resfria (T diminui), a aceitação de pioras cai a quase zero"
    ], top_inch=3.9, font_size=22)
    add_footer(s10, "10")

    # -------------------------------------------------------------
    # SLIDE 11 — PARÂMETROS
    # -------------------------------------------------------------
    s11 = prs.slides.add_slide(blank_layout)
    set_slide_background(s11)
    add_header(s11, "11", "Parâmetros · Resfriamento")
    add_title(s11, "Parâmetros da Têmpera Simulada")

    t11_shape = s11.shapes.add_table(5, 3, Inches(0.8), Inches(2.3), Inches(11.733), Inches(3.0))
    t11 = t11_shape.table
    t11.columns[0].width = Inches(4.5)
    t11.columns[1].width = Inches(2.0)
    t11.columns[2].width = Inches(5.233)

    h11 = ["Parâmetro", "Símbolo", "Definição / Papel"]
    d11 = [
        ("Temperatura inicial", "T₀", "Taxa inicial de exploração de soluções"),
        ("Temperatura final", "Tmin", "Critério de parada do resfriamento"),
        ("Fator de redução", "α", "Esquema geométrico: Tnovo = α · Tatual"),
        ("Critério de vizinhança", "2-opt", "Operador de mutação e perturbação")
    ]
    for c_i, h in enumerate(h11):
        cell = t11.cell(0, c_i)
        cell.fill.solid()
        cell.fill.fore_color.rgb = TEXT_COLOR
        p = cell.text_frame.paragraphs[0]
        p.text = h
        p.font.name = "Arial"
        p.font.size = Pt(16)
        p.font.bold = True
        p.font.color.rgb = WHITE

    for r_i, row in enumerate(d11):
        for c_i, val in enumerate(row):
            cell = t11.cell(r_i + 1, c_i)
            cell.fill.solid()
            cell.fill.fore_color.rgb = SURFACE_COLOR
            p = cell.text_frame.paragraphs[0]
            p.text = val
            p.font.name = "Arial"
            p.font.size = Pt(18)
            p.font.bold = (c_i == 1)
            p.font.color.rgb = ACCENT_COLOR if c_i == 1 else TEXT_COLOR

    c11 = s11.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.6), Inches(11.733), Inches(0.8))
    c11.fill.solid()
    c11.fill.fore_color.rgb = SURFACE_COLOR
    c11.line.color.rgb = BORDER_COLOR
    p = c11.text_frame.paragraphs[0]
    p.text = "Os valores numéricos de T₀, Tmin e α serão definidos e avaliados experimentalmente nos ensaios."
    p.font.size = Pt(16)
    p.font.color.rgb = MUTED_COLOR
    add_footer(s11, "11")

    # -------------------------------------------------------------
    # SLIDE 12 — PSEUDOCÓDIGO
    # -------------------------------------------------------------
    s12 = prs.slides.add_slide(blank_layout)
    set_slide_background(s12)
    add_header(s12, "12", "Implementação · Lógica")
    add_title(s12, "Algoritmo proposto")

    code_bg = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3), Inches(11.733), Inches(4.2))
    code_bg.fill.solid()
    code_bg.fill.fore_color.rgb = DARK_BOX
    code_bg.line.fill.background()

    code_bar = s12.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3), Inches(0.15), Inches(4.2))
    code_bar.fill.solid()
    code_bar.fill.fore_color.rgb = ACCENT_COLOR
    code_bar.line.fill.background()

    tb = s12.shapes.add_textbox(Inches(1.2), Inches(2.4), Inches(11.0), Inches(4.0))
    tf = tb.text_frame
    tf.word_wrap = True
    code_lines = [
        "Gerar solução inicial",
        "Enquanto T > Tmin:",
        "    gerar solução vizinha via 2-opt",
        "    calcular Δ = C(vizinha) − C(solução)",
        "    Se Δ ≤ 0 ou random(0, 1) < exp(−Δ / T):",
        "        solução = vizinha",
        "        atualizar melhor solução encontrada",
        "    reduzir T = α · T",
        "Retornar melhor solução"
    ]
    for i, line in enumerate(code_lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.text = line
        p.font.name = "Consolas"
        p.font.size = Pt(21)
        p.font.bold = ("Enquanto" in line or "Retornar" in line or "Se" in line)
        p.font.color.rgb = ACCENT_COLOR if ("Enquanto" in line or "Retornar" in line or "Se" in line) else WHITE
        p.space_after = Pt(8)

    add_footer(s12, "12")

    # -------------------------------------------------------------
    # SLIDE 13 — APLICAÇÃO NO OPCODA
    # -------------------------------------------------------------
    s13 = prs.slides.add_slide(blank_layout)
    set_slide_background(s13)
    add_header(s13, "13", "Integração · Arquitetura")
    add_title(s13, "Onde o algoritmo entra?")

    # Pipeline em grade
    steps13 = [
        ("1. ENTRADA", ".EXE → Bytes"),
        ("2. PARTICIONAMENTO", "Blocos → Grãos"),
        ("3. OTIMIZAÇÃO", "Têmpera Simulada"),
        ("4. SÍNTESE", "Áudio Granular")
    ]
    for i, (title_st, desc_st) in enumerate(steps13):
        is_opt = (i == 2)
        box = s13.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8 + i * 3.0), Inches(2.3), Inches(2.733), Inches(1.3))
        box.fill.solid()
        box.fill.fore_color.rgb = RGBColor(0xFF, 0xFD, 0xF9) if is_opt else SURFACE_COLOR
        box.line.color.rgb = ACCENT_COLOR if is_opt else BORDER_COLOR
        box.line.width = Pt(2 if is_opt else 1)
        p = box.text_frame.paragraphs[0]
        p.text = title_st
        p.font.name = "Arial"
        p.font.size = Pt(14)
        p.font.bold = True
        p.font.color.rgb = ACCENT_COLOR if is_opt else MUTED_COLOR
        p2 = box.text_frame.add_paragraph()
        p2.text = desc_st
        p2.font.name = "Arial"
        p2.font.size = Pt(18)
        p2.font.bold = True
        p2.font.color.rgb = ACCENT_COLOR if is_opt else TEXT_COLOR

    add_bullets(s13, [
        "A Têmpera Simulada atua na reorganização dos grãos antes da síntese",
        "Não substitui a leitura de bytes, apenas redefine a ordem temporal",
        "O processamento ocorre de forma desacoplada para preservar o tempo real"
    ], top_inch=4.0, font_size=23)
    add_footer(s13, "13")

    # -------------------------------------------------------------
    # SLIDE 14 — O QUE SERÁ AVALIADO
    # -------------------------------------------------------------
    s14 = prs.slides.add_slide(blank_layout)
    set_slide_background(s14)
    add_header(s14, "14", "Metodologia · Avaliação")
    add_title(s14, "Avaliação experimental")
    add_bullets(s14, [
        "Custo da sequência inicial vs. custo da sequência otimizada",
        "Evolução do custo em função do número de iterações",
        "Comportamento da curva de temperatura ao longo do tempo",
        "Comparação de desempenho entre diferentes configurações de parâmetros"
    ], top_inch=2.4, font_size=23)

    c14 = s14.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(5.3), Inches(11.733), Inches(1.1))
    c14.fill.solid()
    c14.fill.fore_color.rgb = SURFACE_COLOR
    c14.line.color.rgb = BORDER_COLOR
    p = c14.text_frame.paragraphs[0]
    p.text = "“Resultados quantitativos ainda dependem da implementação completa e dos experimentos.”"
    p.font.size = Pt(20)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    add_footer(s14, "14")

    # -------------------------------------------------------------
    # SLIDE 15 — CONTRIBUIÇÃO
    # -------------------------------------------------------------
    s15 = prs.slides.add_slide(blank_layout)
    set_slide_background(s15)
    add_header(s15, "15", "Resultados Esperados · Impacto")
    add_title(s15, "O que a abordagem acrescenta ao Opcoda?")
    add_bullets(s15, [
        "Introduz técnicas de otimização combinatória à arquitetura do projeto",
        "Permite estudar formalmente diferentes ordenações de grãos sonoros",
        "Relaciona algoritmos de Programação Linear à computação musical",
        "Cria uma abordagem experimental estruturada para a organização do áudio"
    ], top_inch=2.5, font_size=23)
    add_footer(s15, "15")

    # -------------------------------------------------------------
    # SLIDE 16 — REFERÊNCIAS
    # -------------------------------------------------------------
    s16 = prs.slides.add_slide(blank_layout)
    set_slide_background(s16)
    add_header(s16, "16", "Fundamentação Acadêmica")
    add_title(s16, "Referências")

    refs = [
        "KIRKPATRICK, S.; GELATT, C. D.; VECCHI, M. P. Optimization by Simulated Annealing. Science, 1983.",
        "AARTS, E. H. L.; VAN LAARHOVEN, P. J. M. Simulated Annealing: An Introduction. 1989.",
        "TRIKI, E.; COLLETTE, Y.; SIARRY, P. A Theoretical Study on the Behavior of Simulated Annealing Leading to a New Cooling Schedule. 2005.",
        "ROADS, Curtis. Microsound. MIT Press, 2004."
    ]
    for i, r in enumerate(refs):
        box = s16.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(0.8), Inches(2.3 + i * 1.05), Inches(11.733), Inches(0.9))
        box.fill.solid()
        box.fill.fore_color.rgb = SURFACE_COLOR
        box.line.color.rgb = BORDER_COLOR
        p = box.text_frame.paragraphs[0]
        p.text = r
        p.font.name = "Arial"
        p.font.size = Pt(17)
        p.font.bold = False
        p.font.color.rgb = TEXT_COLOR

    add_footer(s16, "16")

    # -------------------------------------------------------------
    # SLIDE 17 — ENCERRAMENTO
    # -------------------------------------------------------------
    s17 = prs.slides.add_slide(blank_layout)
    set_slide_background(s17)
    add_header(s17, "17", "Conclusão · Agradecimentos")

    t17 = s17.shapes.add_textbox(Inches(0.8), Inches(1.8), Inches(11.733), Inches(1.0))
    p = t17.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Opcoda + Otimização"
    p.font.name = "Arial"
    p.font.size = Pt(44)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR

    # Equação visual
    eq_items = [("DADOS", False), ("+", None), ("OTIMIZAÇÃO", False), ("+", None), ("SÍNTESE GRANULAR", False), ("=", None), ("OPCODA", True)]
    x = 1.0
    for label, is_acc in eq_items:
        if is_acc is None:
            tb = s17.shapes.add_textbox(Inches(x), Inches(3.2), Inches(0.6), Inches(0.9))
            p = tb.text_frame.paragraphs[0]
            p.text = label
            p.font.name = "Arial"
            p.font.size = Pt(28)
            p.font.bold = True
            p.font.color.rgb = ACCENT_COLOR
            x += 0.6
        else:
            w = 2.4 if label == "SÍNTESE GRANULAR" else 1.9
            box = s17.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(3.1), Inches(w), Inches(1.0))
            box.fill.solid()
            box.fill.fore_color.rgb = ACCENT_COLOR if is_acc else SURFACE_COLOR
            box.line.color.rgb = ACCENT_COLOR if is_acc else TEXT_COLOR
            box.line.width = Pt(1.5)
            p = box.text_frame.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER
            p.text = label
            p.font.name = "Arial"
            p.font.size = Pt(16)
            p.font.bold = True
            p.font.color.rgb = WHITE if is_acc else TEXT_COLOR
            x += w + 0.15

    # Autores no final
    c17 = s17.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(3.2), Inches(4.7), Inches(6.933), Inches(1.5))
    c17.fill.solid()
    c17.fill.fore_color.rgb = SURFACE_COLOR
    c17.line.color.rgb = TEXT_COLOR
    c17.line.width = Pt(1.5)
    tb17 = s17.shapes.add_textbox(Inches(3.3), Inches(4.85), Inches(6.7), Inches(1.2))
    p = tb17.text_frame.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    p.text = "Mateus Costa · Rafael Elisei"
    p.font.name = "Arial"
    p.font.size = Pt(22)
    p.font.bold = True
    p.font.color.rgb = TEXT_COLOR
    p2 = tb17.text_frame.add_paragraph()
    p2.alignment = PP_ALIGN.CENTER
    p2.text = "FATEC Cruzeiro · 5º semestre de Análise e Desenvolvimento de Sistemas"
    p2.font.name = "Arial"
    p2.font.size = Pt(16)
    p2.font.color.rgb = MUTED_COLOR

    add_footer(s17, "17")

    prs.save(output_path)
    print(f"Deck gerado com sucesso em: {output_path}")

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "docs/apresentacao-tempera-simulada.pptx"
    create_deck(out)
