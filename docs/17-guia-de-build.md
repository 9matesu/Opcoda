# 17. Guia de build

## Requisitos

- Windows x64
- Visual Studio Build Tools 2026 (toolset v145, MSVC 14.51)
- CMake 3.28 ou superior
- Windows SDK 10.0.26100
- JUCE 8.0.14, clonado em `I:\deps\JUCE` (só para o plugin)

## Dependências

O GoogleTest vem por `FetchContent`, clonado no primeiro configure. O JUCE é
único clone manual, porque é grande e versionado junto com a tag:

```powershell
git clone --depth 1 --branch 8.0.14 --single-branch https://github.com/juce-framework/JUCE.git I:\deps\JUCE
```

O VST3 SDK vem embutido no JUCE, em `modules/juce_audio_processors_headless/
format_types/VST3_SDK`. Não há SDK externo no projeto.

## Compilar

O `cl.exe` depende das variáveis `INCLUDE` e `LIB` que o `vcvars64.bat` define,
e o CMake não as configura sozinho. Por isso o build passa pelo script:

```powershell
.\tools\build.ps1              # debug + AddressSanitizer + testes, sem plugin
.\tools\build.ps1 -Release     # release, com plugin
```

Os executáveis de teste também dependem dessa sessão: o runtime do
AddressSanitizer é uma DLL que fica no PATH do vcvars. Rodar
`build\tests\opcoda_tests.exe` fora dela falha com `STATUS_DLL_NOT_FOUND`, o que
não indica defeito no código.

## Artefatos do plugin

Depois de `-Release`:

```
build-release/src/opcoda_plugin/Opcoda_artefacts/Debug/VST3/Opcoda.vst3
build-release/src/opcoda_plugin/Opcoda_artefacts/Debug/Standalone/Opcoda.exe
```

Para compilar um formato só:

```powershell
cmake --build --preset vst3
cmake --build --preset standalone
```

O VST3 ainda não é copiado para a pasta de plug-ins do sistema, porque
`COPY_PLUGIN_AFTER_BUILD` está desligado. A cópia é feita por
`.\tools\install-vst3.ps1`, que precisa de elevação porque
`C:\Program Files\Common Files\VST3` não é gravável pelo usuário:

```powershell
# copia o bundle para o caminho que o Ableton lê, em uma sessão elevada
.\tools\install-vst3.ps1

# conferir o que seria feito, sem copiar
.\tools\install-vst3.ps1 -WhatIf
```

O caminho do bundle é descoberto por glob, não escrito à mão: o `config` do
diretório de artefatos vem do `CMAKE_BUILD_TYPE`, e foi justamente um caminho
fixo que deixou a instalar a build errada. O script também descarta `*.ilk` e
`*.pdb`, que são resíduo de link e chegavam a 89 MB dentro do bundle.

O caminho por usuário, `%LOCALAPPDATA%\Programs\Common\VST3`, **não** serve no
Windows: o Ableton não o escaneia. Depois de instalar, `Ctrl+F` no Ableton
reindexa os plugins.

## O plugin compila no toolset v145

O `docs/08` previa trocar JUCE por iPlug2 caso o SDK do VST3 travasse. Não foi
necessário: JUCE 8.0.14, anterior ao Visual Studio 2026, compila e linka
perfeitamente contra o toolset v145, inclusive o `juce_vst3_helper` que gera o
`moduleinfo.json` do bundle. A contingência está desativada.

## Presets

| Preset | Compilador | CRT | ASan | Plugin | Uso |
| --- | --- | --- | --- | --- | --- |
| `dev` | MSVC v145 | dinâmico `/MDd` | sim | não | desenvolvimento e portões B, C, D |
| `release` | MSVC v145 | estático `/MT` | não | sim | build de entrega, VST3 e Standalone |

O AddressSanitizer é nativo do MSVC (`/fsanitize=address`) e usa a regra de link
padrão do CMake. Não é preciso clang, lld-link nem qualquer ajuste manual.

O preset `dev` fica com CRT dinâmico porque o ASan do MSVC exige `/MD` ou
`/MDd`. O `release` usa CRT estático de propósito: um VST3 que linka contra
`MSVCP140D.dll` só carrega numa máquina com Visual Studio instalado. Ver
"CMAKE_BUILD_TYPE ausente" abaixo.

## CMAKE_BUILD_TYPE ausente: o plugin que o Ableton não abria

O preset se chamava `release` mas não tinha `CMAKE_BUILD_TYPE`. O gerador é
NMake, que é single-config, e o CMake sem tipo de build no MSVC cai em Debug.
O binário saía em `Opcoda_artefacts/Debug/VST3/` e linkava contra o CRT de
depuração:

```
MSVCP140D.dll   VCRUNTIME140D.dll   VCRUNTIME140_1D.dll   ucrtbased.dll   dbghelp.dll
```

O sintoma era o pior possível. O Ableton lê
`Contents/Resources/moduleinfo.json` do disco para montar a lista de plugins,
então **reconhecia** o Opcoda normalmente. Só depois, para abrir, ele faz
`LoadLibrary` no `.vst3`, que pede as DLL acima. Numa máquina de DAW sem Visual
Studio Build Tools elas não existem, o `LoadLibrary` falha com
`ERROR_MOD_NOT_FOUND` (126), e o plugin ficava listado e não abria.

A correção foram duas linhas no preset: `CMAKE_BUILD_TYPE: Release` e
`CMAKE_MSVC_RUNTIME_LIBRARY: MultiThreaded`. Efeito colateral bom: o binário
caiu de 21,56 MB para 3,84 MB, e `WININET.dll` e `WS2_32.dll` sumiram das
dependências.

**O `tools/build.ps1` agora falha o build** se o VST3 linkar contra qualquer
`*D.dll`, `ucrtbased.dll` ou `dbghelp.dll`. É a rede que impede a regressão:
uma dependência de runtime é o tipo de defeito que só aparece na máquina do
usuário.

Verificar depois de qualquer build de plugin:

```powershell
dumpbin /dependents build-release\src\opcoda_plugin\Opcoda_artefacts\Release\VST3\Opcoda.vst3\Contents\x86_64-win\Opcoda.vst3
```

## Portões de qualidade

| Portão | Como verificar |
| --- | --- |
| A · Build | `.\tools\build.ps1 -Release` compila sem warnings sob `/W4 /WX` e a guarda de runtime de debug passa |
| B · Testes | `.\tools\build.ps1` roda os 87 casos do `opcoda_tests` mais os 6 do guard de alocação, incluindo o T1 e o end-to-end |
| C · Tempo real | `opcoda_alloc_guard_test` e `SampleExchange.AudioThreadAllocatesNothingDuringSwap` provam zero alocação no callback e na troca de material |
| D · Robustez | 10 casos de borda e truncamento, e `notepad.exe` real corrompido |
| E · Interface | verificado na etapa S4, com a GUI |
| F · Documentação | este guia e o README acompanham o código |

## Limitações declaradas

O plano de testes original previa ASan, TSan, UBSan e libFuzzer com clang. O
Windows não tem runtime de ThreadSanitizer em nenhuma distribuição, nem do MSVC
nem do LLVM, e o fuzzing com cobertura não se justifica para um corpus de 60
casos. A toolchain ficou em um compilador só, com o que ele entrega nativamente:

| Ensaio previsto | Situação | Como foi resolvido |
| --- | --- | --- |
| TSan | indisponível no Windows | guard de alocação, ensaio de xrun e revisão por agente |
| UBSan | indisponível no Windows | `/W4 /WX` mais os testes de borda do parser |
| libFuzzer | não adotado | 50 mutações e 10 casos de borda viram testes unitários determinísticos |
| cobertura de branch ≥ 70% | sem medição automática | os testes cobrem os caminhos de erro; a métrica automática espera |
| `pluginval` | travado nesta máquina | ver abaixo |

Essas limitações não escondem defeito: o portão C é satisfeito por um teste que
falha se `processBlock` alocar, e o portão D por testes que rejeitam toda entrada
malformada com erro tipado. O que falta é a medição numérica da cobertura, e ela
volta quando houver clang disponível para o `llvm-cov`.

## pluginval trava nesta máquina

O `pluginval` da Tracktion, versão 0.2.7, foi baixado para `I:\tools\pluginval`
e instalado corretamente, mas trava ao validar o bundle. Ele fica com 0,03 s de
CPU em 25 minutos e não escreve uma linha em `stdout`: está bloqueado esperando
algo, provavelmente um diálogo modal que não aparece nesta sessão. Não é um
problema do Opcoda, e não trava nem no `--strictness-level=1` nem com
`--skip-gui-tests`.

A conformidade do bundle é verificada de outro jeito: o `moduleinfo.json`
gerado pelo `juce_vst3_helper` é conferido direto, e é ele que declara
`Sub Categories` e `Class Flags`. Para os testes que o `pluginval` faria, a
validação real é abrir no Ableton.

Se o travamento incomodar, o caminho é rodar o `pluginval` em outra máquina ou
em sessão de console interativa.

## Corrigir o valor de R do DC-blocker

Resolvido. `docs/05-fundamentacao.md`, o artigo, `04-objetivos.md` e
`06-metodologia-arquitetura.md` indicavam R = 0,995 com corte entre 10 e 15 Hz.
Esses dois números não são compatíveis: R = 0,995 corta em 35,2 Hz a 44,1 kHz.
O código e os quatro textos usam R = 0,9983, que produz o corte de 12 Hz, com a
relação R = exp(−2·π·f_c/f_s) explicitada na fundamentação.
