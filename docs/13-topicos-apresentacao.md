# 13. Tópicos para apresentação: databending, justificativas, diferencial e similares

Roteiro em blocos para apresentação de cerca de 10 minutos, com 1 minuto por bloco e o centro no diferencial e na demonstração.

## 1. Abertura

- Título, equipe, curso e entrega do MVP em 03/11
- Uma frase: o que o Opcoda faz (transforma .exe em som tocável)

## 2. Databending

- Conceito: ler arquivo não sonoro como se fosse áudio
- Como o Audacity faz hoje (import bruto, offline)
- O que se ouve ao abrir um .exe cru

## 3. Por que o bruto falha

- Offset DC: bytes não têm média zero, o sinal desloca
- Transientes: cabeçalhos criam cliques sem valor musical
- Aliasing: variações rápidas demais dobram para o audível
- Risco: parser ingênuo lê fora dos limites e derruba o DAW

## 4. Justificativas (por que é TG de ADS)

- Separação UI vs. DSP e tempo real rígido
- Fila lock-free e memória estática no loop de áudio
- Parser seguro com PROT_READ e bounds checking

## 5. Similares

- Granulator II: referência granular, mas só WAV e só Ableton
- Quanta 2: granular profissional VST3/CLAP, mas só samples
- Audacity raw: lê qualquer arquivo, mas offline e sem tratamento

## 6. Diferencial do Opcoda

- Único que junta parser PE seguro, entropia como modulação e granular em tempo real
- Tabela comparativa de 1 slide (a mesma do `09-similares.md`)

## 7. Como funciona (arquitetura)

- Pipeline em 4 blocos: Parser, Fila SPSC, Motor granular, Saída
- Entropia guiando dispersão dos grãos; DC-blocker e limiter na saída
- Diagrama BPMN como apoio visual (`docs/opcoda-fluxo.bpmn`)

## 8. W3C e acessibilidade

- Técnica aplicada: WCAG 2.2 nível AA pelo método WCAG-EM do W3C
- Escopo: docs do projeto (auditados) + GUI do plugin (critérios de aceite)
- Achado real nos docs: links genéricos reprovam no critério 2.4.4, já corrigidos com texto descritivo
- Tabela de cronograma aprovada no 1.3.1; `lang="pt-BR"` e contraste 4.5:1 como pendências verificáveis
- GUI: teclado total (2.1.1), foco visível (2.4.7), nomes acessíveis nos controles (4.1.2), erro do parser em texto e nunca só por cor (3.3.1)
- Reauditoria marcada para o fim da etapa E4, antes do congelamento

## 9. MVP e validação

- Escopo até 03/11: standalone + VST3 x64, arrasto, 8 vozes, MIDI
- Critérios: menos 40 dB de DC, menos de 50% do budget, 0 segfaults, arrasto com som em menos de 2 s
- Próximos passos: CLAP, bateria completa, monografia
- Fechamento com a hipótese H1 em uma frase
