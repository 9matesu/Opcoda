# Guia do usuário

## Instalação

1. Compile a configuração Release com `.\tools\build.ps1 -Release`.
2. Abra um PowerShell elevado e rode `.\tools\install-vst3.ps1`.
3. O instalador copia o VST3 para `C:\Program Files\Common Files\VST3`.
4. No host, faça uma nova varredura de plugins.

O Standalone fica em
`build-release\src\opcoda_plugin\Opcoda_artefacts\Release\Standalone\Opcoda.exe`
e abre sem instalação.

## Carregar material

Arraste um `.exe`, `.dll`, `.bin` ou `.sys` para a janela. O ícone de pasta abre
o seletor de arquivos. O carregamento ocorre em menos de um segundo para
arquivos pequenos.

O parser recusa arquivos inválidos com um código:

| Código | Significado |
| --- | --- |
| `E_BAD_MZ` | Falta a assinatura MZ. |
| `E_BAD_PE` | Falta a assinatura PE. |
| `E_TRUNCATED` | O arquivo termina antes do cabeçalho ou da seção. |
| `E_OOB` | Uma seção aponta para fora do arquivo. |
| `E_TOO_MANY_SECTIONS` | O número de seções passa do limite. |

Quando o arquivo é recusado, o material anterior continua tocando.

## Primeiro som

1. Carregue um arquivo.
2. Pressione `Espaço` para ouvir a varredura da região.
3. Toque uma nota no teclado virtual ou envie MIDI.
4. Ajuste `SIZE` para 40 ms e `DENSITY` para 20 /s como ponto de partida.
5. Mova `POSITION` para escolher a região.
6. Suba `SPRAY` para espalhar os grãos.

## Vistas

| Tecla | Vista | Uso |
| --- | --- | --- |
| `1` | WAV | Forma de onda da região. |
| `2` | HEX | Grade de bytes; clique move a cabeça de leitura. |
| `3` | ENT | Curva de entropia por coluna. |

Na vista HEX, as setas movem o cursor um byte, `Shift` com as setas move 16,
`PgUp`/`PgDn` movem 1024 e `Início`/`Fim` vão aos extremos. `Enter` ativa o byte
sob o cursor. `Esc` devolve o cursor à cabeça de leitura. `A` alterna entre a
região exata e a seção PE mais próxima.

## Teclado

O teclado virtual cobre cinco oitavas a partir de Dó3. As teclas do computador
`A W S E D F T G Y H U J K` tocam notas. `K` também leva o foco ao teclado.

O módulo `KEYS` quantiza a nota para a escala e a tônica escolhidas. `GLIDE`
desliza entre notas no tempo definido. O pedal de sustain segura a nota.

## Presets

O seletor no topo lista seis presets de fábrica: Init, Cloud, Shimmer, Pulse,
Drone e Grit. As setas `<` e `>` percorrem a lista. O menu salva e carrega
arquivos `.opcoda`. O asterisco ao lado do nome indica alteração não salva.

O estado do projeto guarda o material, a região, os parâmetros e a matriz de
modulação.

## Solução de problemas

**O Standalone abre com valores estranhos.** Apague
`%APPDATA%\Opcoda\Opcoda.settings` e abra de novo.

**Não há som.** Verifique se o material foi aceito, se o transporte está ligado
ou se uma nota está ativa. Confira a saída de áudio do Standalone.

**O host não encontra o VST3.** Rode o instalador como administrador e confirme
o caminho `C:\Program Files\Common Files\VST3`.

**O arquivo é recusado.** O Opcoda lê apenas PE válido. Arquivos truncados ou
com seções fora dos limites retornam erro tipado.
