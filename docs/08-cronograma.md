# 8. Cronograma de execução estimado

Entrega MVP em 03/11. CLAP, bateria completa de testes e monografia seguem após a entrega.

## Situação em 19/10/2026

O cronograma original pressupunha GUI só na etapa S4, depois de MIDI. A ordem
real foi invertida por uma razão de risco: sem um plugin que abra no host não há
como validar arrasto, parâmetros nem MIDI, que é justamente o que o T4 mede. A
GUI do chassi foi antecipada para dar um alvo verificável mais cedo, e a etapa
S3 continua inteira.

Em 19/10 o host licenciado passou a existir, o que retirou o bloqueio do T2 e do
T4, e a emenda 1.2.0 abriu as features F018–F024. O cronograma abaixo é o único
que vale: o de 03/10 está superado.

| Etapa | Estado | O que falta |
| --- | --- | --- |
| S1 esqueleto e parser | concluída | nada |
| S2 motor, arrasto e DC-blocker | concluída | nada |
| S3a MIDI, estado e guardas | concluída | nada: estado do host, gate por note-on, pedal de sustain e 2 CCs mapeados |
| S3b T2 no Ableton | **a executar** | medir p99 e confirmar zero xruns em 5 minutos, por grão × densidade; o host existe desde 19/10 |
| S4a GUI e display | concluída | nada: chassi, knobs, LEDs, grelha de bytes, campo de endereço e telemetria |
| S4b T4 no Ableton | **a executar** | arrasto, os 41 parâmetros, os 2 CCs, 2 minutos por formato, e a ida e volta do estado |
| S4c instrumento v1.2.0 | por fazer | F017 a F024 conforme a emenda 1.2.0 |
| S5 congelamento e validação | por fazer | auditoria de documentação, artigo e monografia reescritos, pacote x64, T2 e T4 finais |

Duas etapas estão fora da ordem original e isso é uma decisão registada, não um
atraso escondido. O que mede se é a validade da cadeia inteira, e uma cadeia que
não abre no host não valida nada.

**S3b e S4b deixaram de estar bloqueadas em 19/10/2026.** O T2 e o T4 medem-se
num DAW, e o host licenciado existe nesta máquina. A versão anterior desta secção
dizia o contrário — que a cópia não tinha licença e nunca abria janela — e essa
afirmação passou a ser falsa; a correção está registada em `docs/07-plano-testes.md`.

## Plano de 19/10 a 03/11

| Frente | Até | Foco | Entrega |
| --- | --- | --- | --- |
| Emenda + specs | 20/10 | constitution 1.2.0, registro no `docs/18`, specs F018–F024, `docs/19` | tabela de 41 parâmetros fechada |
| Validação do build atual | 21/10 | instalar o bundle, T4, ida e volta do estado, baseline do T2 | método do T2 provado antes da reescrita |
| Núcleo | 27/10 | F018 teclado e afinação · F019 LFO rítmico · F020 analógico e matriz · F021 nuvem | testes por parâmetro, guard verde |
| Interface | 31/10 | F022 waveform PCM sobre a F017 · F023 LookAndFeel v2 e visualizadores · F024 OpenGL | UI v2 com queda para software |
| S5 | 03/11 | congelar, sincronizar docs, reescrever artigo e monografia, empacotar | pacote x64 e T2/T4 finais |

O `docs/08` não é um compromisso de data que se renova sozinho. Se o T2 ou o T4
revelarem defeito, a etapa volta ao estado "por fazer" a partir do achado, e a
tabela acima é o ponto honesto para registar isso. A ordem de corte, se alguma
frente atrasar, é F024 → F021 → o caráter do filtro na F020 → a multi-resolução
da F022.

## Contingências

Grão acima de 100 ms com interpolação linear se houver dropout, como previsto no
plano original. As 50 mutações de bit-flip do corpus do T3 e os 10 casos de borda
estão automatizados, cada mutação com o erro tipado que se espera dela.

Contingência desativada em 01/10: a troca de JUCE por iPlug2 estava prevista caso
o SDK do VST3 travasse, e JUCE 8.0.14 compila e linka no toolset v145 do Visual
Studio 2026, inclusive o gerador de manifest do bundle. O `pluginval`, esse sim,
trava nesta máquina, e o T2 passa a ser verificado no Ableton. Detalhes em
`docs/17-guia-de-build.md` e `docs/07-plano-testes.md`.