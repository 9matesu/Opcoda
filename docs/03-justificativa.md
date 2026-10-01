# 3. Justificativa e motivação técnica

Para alunos de ADS, o valor do Opcoda está na arquitetura. O som é só parte do resultado.

## Separação entre interface e DSP

Plugin de áudio tem duas threads com regras distintas. A thread de interface pode alocar memória, abrir arquivos e demorar. A thread de áudio é chamada a cada 128 samples, cerca de 2,9 ms a 44,1 kHz, e não pode alocar, travar ou fazer I/O. O projeto exige respeitar essa divisão na prática.

## Comunicação lock-free

A interface avisa o DSP por fila SPSC de produtor único e consumidor único, com operações atômicas. Nada de mutex no callback process. Parâmetros chegam como valores atômicos com suavização.

## Memória estática no DSP

Todas as vozes, envelopes e filtros são alocados na inicialização. No loop de áudio não há new, malloc ou redimensionamento de vetor.

## Leitura com permissão mínima

O arquivo é mapeado somente para leitura, copiado para um buffer próprio e fechado em seguida. Validação de assinatura MZ e PE, checagem de limites para cada seção. Nenhum trecho é marcado como executável.

## Ineditismo

- Audacity raw import: offline e sem tratamento. Não é instrumento em tempo real.
- Samplers granulares comerciais (Portal, Padshop 2, Fragments): só aceitam WAV e não entendem estrutura PE.
- Sonificação forense (BinVis e similares): trabalho offline em Python, sem requisitos de tempo real.

Na prática, isso significa um parser PE seguro de um lado e um motor granular em tempo real do outro.
