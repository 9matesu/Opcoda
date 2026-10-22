# 022 · Waveform PCM do ficheiro inteiro

**Feature Branch**: `022-waveform-pcm`
**Criada**: 2026-10-19
**Status**: Draft
**Objetivos atendidos**: OE7 (forma de onda navegável)
**Ensaios**: portões B, C e E
**Depende de**: F017 (leitura em blocos e cancelável)

**Alvo**: F022, do mapa da emenda 1.2.0

## Contexto

O display atual desenha a **região carregada**, e desenha-a a partir dos bytes
convertidos. Para um ficheiro de 4 KB isso é honesto; para um `.exe` de 12 MB
com uma região de 200 bytes, o utilizador vê 200 bytes e não vê o ficheiro.

Um DAW mostra o ficheiro inteiro porque o ficheiro inteiro é a unidade de
navegação. Aqui a unidade também é: o produtor escolhe uma região de um artefacto,
e para escolher precisa de ver onde estão as seções, onde está o código, onde está
o recurso comprimido. A curva de entropia já dá essa visão em forma abstrata; a
waveform dá-a em forma de forma de onda, que é o que um produtor sabe ler.

Esta feature constrói, durante a leitura da F017, uma pirâmide de picos
(min/max por bloco) com níveis de resolução em potências de 2, e desenha o nível
adequado ao zoom. É a mesma técnica de um editor de áudio, e o custo é uma
passagem pelo ficheiro que a F017 já faz.

## Cenários

### História 1 — Ver o ficheiro inteiro (P1)

Como produtor, quero ver a forma de onda do ficheiro inteiro e onde está a região
que escolhi, para me orientar no artefacto.

**Por que P1**: sem isto, a escolha de região é às cegas, e o instrumento perde a
única navegação espacial que tem.

**Teste independente**: `Waveform.FullFilePeaksMatchBruteForce` compara a
pirâmide de picos com o cálculo força-bruta sobre 1 MB de dados, com tolerância
de 1e-6.

### História 2 — Zoom até à amostra (P1)

Como produtor, quero ampliar até ver amostras individuais, para escolher um ponto
exato.

**Teste independente**: `Waveform.ZoomToSampleSelectsLevelZero` confirma que, com
menos de 256 amostras visíveis, o nível desenhado é o 0 e o número de pontos é
igual ao de amostras.

### História 3 — Desenhar a região com o rato (P2)

Como produtor, quero arrastar sobre a waveform para definir a região, em vez de
escrever o endereço.

**Teste independente**: `Waveform.DragSelectsRegionWithBytePrecision` converte
um arrasto de 100 px para os offsets corretos e confirma o arredondamento.

### História 4 — Não travar com 200 MB (P1)

Como utilizador, quero abrir um ficheiro grande sem congelar a interface.

**Teste independente**: `Waveform.PyramidBuildIsCancelable` cancela a construção
a meio e confirma que não fica memória pendente nem pico parcial publicado.

## Requisitos

1. **Pirâmide de picos.** Nível 0: 256 amostras por par (min, max). Nível *k*:
   agregado do nível *k−1*, com metade dos pontos. Constrói-se até haver menos de
   2 pontos.
2. **Construção durante a leitura.** A pirâmide é construída no thread da F017, à
   medida que os blocos chegam. Cancelar descarta tudo.
3. **Teto de memória.** A pirâmide ocupa no máximo 16 MB; acima disso, o nível
   mais fino é descartado até caber, e a interface declara a resolução efetiva.
4. **Fonte.** As amostras são as mesmas que o motor lê — a conversão
   byte→amostra do parser, não uma segunda conversão.
5. **Desenho.** Escolhe-se o nível em que o número de pontos é pelo menos o dobro
   dos pixels disponíveis; desenha-se min/max por coluna. Sem alocação por
   quadro: os pontos vivem em buffers reutilizados.
6. **Zoom por rato.** Roda do rato amplia em torno do cursor; arrasto com o botão
   esquerdo define a região; `Shift` + arrasto move a vista.
7. **Régua.** Escala em bytes, com marcas legíveis em potências de 10 e de 2, e o
   endereço sob o cursor.
8. **Marcas de seção.** As seções PE aparecem como faixas finas acima da
   waveform, clicáveis, com o nome truncado.
9. **A região escolhida é visível** com contraste ≥ 3:1 contra o fundo, e não só
   por cor — tem também um contorno e um rótulo.
10. **Sem regressão da entropia.** A curva de entropia continua a existir, agora
    alinhada com a waveform no mesmo eixo de bytes.

## Critérios de aceite

- [ ] picos do nível 0 iguais ao cálculo força-bruta;
- [ ] zoom até 64 amostras usa o nível 0 e desenha 64 pontos;
- [ ] arrasto de 100 px produz offsets corretos em ficheiro de 1 MB e de 200 MB;
- [ ] pirâmide de 200 MB fica abaixo de 16 MB e a interface declara a resolução;
- [ ] cancelar a construção não deixa pico parcial nem memória pendente;
- [ ] a interface não bloqueia mais de 16 ms por quadro durante o zoom;
- [ ] a região é distinguível sem cor (contorno e rótulo);
- [ ] guard de alocação verde durante a reprodução com a waveform a desenhar.

## Não-objetivos

- **Forma de onda do áudio sintetizado.** A waveform é do material, não da saída.
- **Edição destrutiva.** Não se apaga nem se escreve no binário.
- **Cache em disco.** A pirâmide vive em memória durante a sessão.
- **Waveform do sinal depois dos filtros.** Não é o que se está a escolher.

## Testes

| Teste | O que prova |
| --- | --- |
| `Waveform.FullFilePeaksMatchBruteForce` | correção da pirâmide |
| `Waveform.LevelSelectionForZoom` | escolha de nível |
| `Waveform.ZoomToSampleSelectsLevelZero` | zoom máximo |
| `Waveform.DragSelectsRegionWithBytePrecision` | seleção por arrasto |
| `Waveform.PyramidBuildIsCancelable` | cancelamento |
| `Waveform.MemoryStaysUnderCap` | teto de memória |
| `Waveform.RegionVisibleWithoutColour` | acessibilidade |
