# 8. Cronograma de execução estimado

Entrega MVP em 03/11. CLAP, bateria completa de testes e monografia seguem após a entrega.

| Semana | Até | Foco | Entrega |
| --- | --- | --- | --- |
| S1 | 08/10 | Esqueleto do plugin + parser PE em teste | áudio passando com 1 parâmetro; parser lendo .exe |
| S2 | 15/10 | Motor 8 vozes + arrasto + DC-blocker | primeiro som de .exe no standalone |
| S3 | 22/10 | MIDI learn e CC + T2 enxuto | performance via MIDI sem xruns |
| S4 | 29/10 | GUI simples + T1 + fuzzing básico | build candidata VST3 x64 |
| S5 | 03/11 | Congelamento + validação no REAPER | pacote x64 entregue |

Contingências: priorizar CLAP com nih-plug se o SDK VST3 travar vira trocar JUCE por iPlug2 na S1. Travar o grão em 100 ms com interpolação linear se houver dropout. Começar o fuzzing pelos 10 casos de borda se o corpus atrasar.
