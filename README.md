<p align="center">
  <img src="resources/icons/opcoda-icon-256.png" width="96" alt="Opcoda">
</p>

<h1 align="center">Opcoda</h1>

<p align="center">
  <strong>Sintetizador granular que lê executáveis como matéria-prima sonora.</strong><br>
  VST3 e standalone para Windows x64 &middot; versão 0.1.0
</p>

![Interface do Opcoda com um binário carregado](docs/assets/screenshots/opcoda-ui.png)

## O instrumento

O Opcoda lê a estrutura de um arquivo PE (`.exe`, `.dll`, `.bin`, `.sys`), converte
os bytes em amostras e os reproduz com um motor granular de oito vozes. O grão vai
de 1 ms a 2 s. Cada região do arquivo tem uma assinatura estatística própria, e o
instrumento expõe essa leitura em quatro vistas: forma de onda, grade de bytes com
desmontagem, curva de entropia e matriz de modulação.

O som não vem de um sample. Vem do material: o cabeçalho, o código, os dados. É um
instrumento de textura, ruído e ritmo, e não um reprodutor de arquivos.

## O que está incluído

- Motor granular de oito vozes, com janelas Hann, Gauss, Hamming e Blackman.
- 61 parâmetros automatizáveis, do tamanho do grão ao LFO e à matriz de modulação.
- Dois filtros biquad em série, envelope ADSR com curvas e oito rotas de modulação.
- Quatro vistas do material, incluindo a coluna ASM que desmonta código 64 bits.
- Transporte sem MIDI, teclado virtual e controle por MIDI.
- Presets de fábrica e arquivos `.opcoda` para guardar o estado.
- Cinco temas: Grafite, Clássico, Claro, Âmbar e Ciano.

## A interface

| `ENV`: ADSR desenhado, com o ponto vivo | `FILTER`: resposta dos dois biquads |
| --- | --- |
| ![Envelope com o ponto vivo no patamar do sustain](docs/assets/screenshots/opcoda-env.png) | ![Resposta dos filtros com as pegas de corte e Q](docs/assets/screenshots/opcoda-filter.png) |

| `HEX`: bytes, ASCII e ASM | `ENTROPY` com zoom |
| --- | --- |
| ![Grade de bytes com a coluna ASM a desmontar o código](docs/assets/screenshots/opcoda-hex.png) | ![Curva de entropia com zoom, em janelas de 256 bytes](docs/assets/screenshots/opcoda-entropy.png) |

O ecrã é uma chapa escura com um rail de navegação à esquerda. O painel de
parâmetros fica por baixo, com a coluna master à direita e o teclado no pé. Tudo
assenta numa grelha de 8 px: margens de 16, painéis de 6 px de raio e filetes de
1 px. Os detalhes estão em `design/DESIGN-SYSTEM.md`.

## Temas

O tema muda o instrumento inteiro, do chassis ao último rótulo, e fica guardado.

| Grafite | Clássico | Claro |
| --- | --- | --- |
| preto e branco, sem cor | creme, carvão e laranja queimado | papel, grafite e azul cobalto |
| `#0A0A0A` | `#D6C9AE` | `#F7F4EE` |

| Âmbar | Ciano |
| --- | --- |
| grafite com âmbar | grafite com ciano |
| `#FFA726` | `#35D0E0` |

O desenho é monocromático em todos eles. A cor fica no acento, não no fundo.

## Requisitos

- Windows 10 ou 11, 64 bits.
- 4 GB de RAM.
- Host VST3 opcional (Ableton Live, Reaper, FL Studio).
- O standalone funciona sozinho, sem host.

## Instalação

1. Baixe o instalador `Opcoda-Setup-0.1.0.exe` na pasta `installer/Output`.
2. Execute e escolha os componentes: VST3, standalone e manual.
3. O VST3 é instalado em `C:\Program Files\Common Files\VST3`.

O instalador oferece o runtime do Visual C++ quando ele falta na máquina. A
instalação é por máquina e pede elevação.

## Licença

O uso é pessoal, intransferível e não exclusivo. Você pode usar o instrumento em
produções próprias e publicar o áudio resultante sem pagar royalties. Não é
permitido redistribuir, sublicenciar ou engenharia reversa do software. O texto
completo está em `installer/LICENSE.txt`.

## Créditos

Referências de síntese granular: Granulator II (Robert Henke) e Quanta 2.
Sistema de design e interface: `design/DESIGN-SYSTEM.md`.