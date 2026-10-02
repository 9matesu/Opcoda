# 8. Cronograma de execução estimado

Entrega MVP em 03/11. CLAP, bateria completa de testes e monografia seguem após a entrega.

## Situação em 02/10/2026

O cronograma original pressupunha GUI só na etapa S4, depois de MIDI. A ordem
real foi invertida por uma razão de risco: sem um plugin que abra no host não há
como validar arrasto, parâmetros nem MIDI, que é justamente o que o T4 mede. A
GUI do chassi foi antecipada para dar um alvo verificável mais cedo, e a etapa
S3 continua inteira.

| Etapa | Estado | O que falta |
| --- | --- | --- |
| S1 esqueleto e parser | concluída | nada |
| S2 motor, arrasto e DC-blocker | concluída | nada |
| S3 MIDI e T2 | por fazer | gate por note-on, 2 CCs mapeados, medição de p99 no Ableton |
| S4 GUI, T1 e robustez | parcial | chassi, knobs e LEDs prontos; falta o display de entropia e a telemetria |
| S5 congelamento e validação | por fazer | auditoria de documentação, pacote x64 |

Duas etapas estão fora da ordem original e isso é uma decisão registada, não um
atraso escondido. O que mede se é a validade da cadeia inteira, e uma cadeia que
não abre no host não valida nada.

## Plano a partir de 03/10/2026

| Semana | Até | Foco | Entrega |
| --- | --- | --- | --- |
| S3a | 09/10 | guardar estado dos parâmetros, guarda de CMake, gate por note-on e 2 CCs | teclado controla o som, CC move parâmetro |
| S3b | 16/10 | T2 no Ableton Live 12 com buffers de 128, 256 e 512 a 44,1 e 48 kHz | p99 medido, zero xruns em 5 minutos |
| S4a | 23/10 | display de entropia com brilho, abas de seção e telemetria | a peça visual que falta |
| S4b | 30/10 | T4 no Ableton: arrasto, os 6 parâmetros, os 2 CCs, 2 minutos por formato | ensaio executado com evidência |
| S5 | 03/11 | congelamento, auditoria de documentação e empacotamento | pacote x64 entregue |

O `docs/08` não é um compromisso de data que se renova sozinho. Se o T2 ou o T4
revelarem defeito, a etapa volta ao estado "por fazer" a partir do achado, e a
tabela acima é o ponto honesto para registar isso.

## Contingências

Grão acima de 100 ms com interpolação linear se houver dropout, como previsto no
plano original. As 50 mutações de bit-flip do corpus do T3 entram como testes
unitários determinísticos quando o corpus existir; os 10 casos de borda já estão
automatizados.

Contingência desativada em 01/10: a troca de JUCE por iPlug2 estava prevista caso
o SDK do VST3 travasse, e JUCE 8.0.14 compila e linka no toolset v145 do Visual
Studio 2026, inclusive o gerador de manifest do bundle. O `pluginval`, esse sim,
trava nesta máquina, e o T2 passa a ser verificado no Ableton. Detalhes em
`docs/17-guia-de-build.md` e `docs/07-plano-testes.md`.