# 7. Plano de testes e critérios de validação

## T1. Atenuação de DC e ruído residual

Método: três binários de referência no MVP, incluindo notepad.exe, um binário comprimido com UPX e um arquivo sintético zerado. Captura de 10 segundos com grão de 40 ms e densidade média. FFT com N 65536 e janela Blackman-Harris.

Critério: atenuação de ao menos 40 dB no bin DC em relação à leitura bruta e energia abaixo de 20 Hz menor que -60 dBFS.

## T2. Latência e CPU sob polifonia

Método: sessão no Ableton Live 12.3.1 com buffers de 128, 256 e 512 samples a 44,1 e 48 kHz. Medição de tempo médio e p99 do process pela tela de latência do Ableton, mais carga de CPU do sistema. Casos com 1, 4 e 8 vozes e grãos de 10 e 100 ms.

Critério: p99 abaixo de 50% do orçamento (2,9 ms para 128 samples a 44,1 kHz) e zero xruns em 5 minutos contínuos.

## T3. Estresse com binários corrompidos

Método: corpus enxuto no MVP com 50 mutações por bit-flip, truncamento e cabeçalhos forjados, mais 10 casos de borda como arquivo vazio, 1 byte e e_lfanew fora do arquivo. Execução em loop do parser com host de teste.

Critério: zero segfaults, zero leaks, 100% das entradas inválidas rejeitadas com erro tipado e cobertura de branch do parser acima de 70%.

## T4. Aceite de uso (arrasto, parâmetros e MIDI)

Método: arrastar um .exe para o Standalone e para o VST3 no Ableton Live 12.3.1, mover os 6 parâmetros principais, mapear 2 CCs e tocar 2 minutos em cada formato.

Critério: som sai em menos de 2 s após o arrasto, zero xruns e todo CC mapeado move o parâmetro correspondente.

Complemento qualitativo: teste de usabilidade com 5 usuários e escala SUS, mais escuta comparativa entre leitura bruta e saída tratada.

## Limitações da plataforma de validação

O Windows não distribui runtime de ThreadSanitizer, nem no MSVC nem no LLVM, e o AddressSanitizer nativo do MSVC não cobre comportamento indefinido. O projeto ficou com um compilador só e o que ele entrega: ASan nativo, `/W4 /WX` e testes determinísticos.

Consequência sobre os ensaios acima:

| Ensaio | Situação | Como é verificado |
| --- | --- | --- |
| T1 (espectral) | executado | FFT N 65536 com Blackman-Harris em teste automatizado, critério de 40 dB e de -60 dBFS |
| T2 (latência) | bloqueado | p99 medido em teste; o ensaio com DAW e 8 vozes reais exige um host licenciado, que não está disponível nesta máquina |
| T3 (robustez) | executado | 10 casos de borda e truncamento automatizados; as 50 mutações de bit-flip entram como testes quando o corpus existir |
| T4 (aceite) | bloqueado | exige instalar o bundle atual num DAW licenciado e repetir arrasto, parâmetros e CCs, etapas S3b e S4b |
| TSan | indisponível | substituído pelo guard de alocação, que falha se o callback alocar |
| UBSan | indisponível | substituído por `/W4 /WX` e pelos testes de borda |
| `pluginval` | indisponível | trava nesta máquina; a conformidade do bundle é conferida no `moduleinfo.json` gerado pelo `juce_vst3_helper`, e o comportamento é medido no Ableton |
| cobertura de branch ≥ 70% | sem métrica automática | os testes exercitam os caminhos de erro; a medição numérica espera toolchain com llvm-cov |

## Por que Ableton Live e não REAPER

O plano original nomeava REAPER 7 e Bitwig Studio como hosts de T2 e T4, e o
`pluginval` como critério do T2. Nenhum dos três está disponível na máquina de
desenvolvimento, e o `pluginval` da Tracktion trava mesmo com
`--strictness-level=1 --skip-gui-tests`, ficando com 0,03 s de CPU em 25 minutos
sem escrever uma linha em `stdout`.

Ableton Live 12.3.1 é o host que o utilizador vai usar de facto, e dá telemetria
de latência suficiente para o critério do T2. Trocar o host é uma mudança de
método, não um critério mais frouxo: o número continua a ser p99 abaixo de 50% do
orçamento com zero xruns.

**Este é o que bloqueia o T2 e o T4 hoje.** Existe uma cópia do Ableton nesta
máquina, mas não é uma instalação licenciada: o diretório de programa traz um
keygen e um patcher, e o executável arranca mas nunca abre uma janela utilizável,
porque a ativação não está feita. Sem chave legítima não há ensaio, e ativar com
o material que acompanha o pacote não é uma opção.

Por isso o T2 e o T4 ficam **bloqueados** e não "pendentes". A diferença é
deliberada: pendente é trabalho que está agendado; bloqueado é trabalho que não
pode ser feito com o que existe nesta máquina. Um T2 executado num host
licenciado é o que fecha o portão C no Ableton; sem ele, o portão continua a
passar no guard de alocação e nos testes, e a lacuna fica escrita.

O que já foi verificado sem DAW: o bundle declara `["Instrument", "Synth"]` no
`moduleinfo.json`, e o `LoadLibrary` foi exercitado num host real quando o
plugin foi aceite numa sessão anterior. O que falta é a telemetria de latência.

O `pluginval` sai do critério com substituto declarado, não com o critério
apagado. O que ele verificaria é conferido direto no `moduleinfo.json`, que é o
que o host lê para decidir a categoria do plugin, e o comportamento seria medido
no Ableton, se e quando houver host licenciado.

Nenhuma dessas lacunas é omitida: a verificação de concorrência é feita por
teste automatizado, e não por declaração.
