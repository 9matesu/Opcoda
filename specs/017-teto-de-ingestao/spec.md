# 017 · Teto de ingestão e análise cancelável

**Feature Branch**: `017-teto-de-ingestao`
**Criada**: 2026-10-19
**Status**: Draft
**Objetivos atendidos**: OE2 (leitura verificada), OE3 (fila), OE8 (troca sem
bloqueio)
**Ensaios**: portões B, C e D

**Alvo**: F017, criada pela emenda 1.1.0 e nunca implementada

## Contexto

A constitution pede, no §3 e no §11, um teto de ingestão e uma análise
cancelável. O código não tem nenhum dos dois: `PluginProcessor::ingest`
(`plugin_processor.cpp:144-165`) lê o ficheiro inteiro de forma síncrona, no
thread que chamou o método, sem limite de tamanho e sem forma de interromper.

Isso é aceitável para um `.exe` de 100 KB e inaceitável para um binário de
800 MB: a janela congela durante a leitura, o parser percorre a tabela de seções
de um ficheiro que não cabe em memória confortável, e não há botão que pare.
Além disso, a F022 (waveform PCM do ficheiro inteiro) precisa de uma passagem
pelo ficheiro inteiro para construir os níveis de resolução — e essa passagem
não pode acontecer no thread da mensagem.

Esta feature não muda o formato do áudio: muda onde e como o ficheiro é lido.

## Cenários

### História 1 — O ficheiro grande é recusado com um número (P1)

Como utilizador, quero que um ficheiro acima do teto seja recusado com o limite
escrito na mensagem, para saber se o problema é o tamanho ou o formato.

**Por que P1**: hoje o plugin tenta ler tudo e a interface congela sem explicação.
Recusar com número é o comportamento mínimo honesto.

**Teste independente**: `Ingest.OversizeFileReturnsTooLargeWithLimit` confirma,
num núcleo sem JUCE, que um ficheiro acima do teto devolve `E_TOO_LARGE` e que a
mensagem contém o teto em MB.

### História 2 — A leitura pode ser cancelada (P1)

Como utilizador, quero cancelar uma leitura a meio, porque arrastei o ficheiro
errado e não quero esperar.

**Por que P1**: sem cancelamento, a única saída é fechar o host.

**Teste independente**: `Ingest.CancelStopsWithinOneChunk` verifica que o
cancelamento é observado num prazo de um bloco de leitura e que nenhum material
parcial é publicado na fila.

### História 3 — O áudio não é tocado pela leitura (P1)

Como host, não quero que a leitura de um ficheiro altere o estado do motor.

**Teste independente**: `Ingest.CancelLeavesPreviousMaterialPlaying` mantém o
material anterior a tocar durante e depois de uma leitura cancelada.

## Requisitos

1. **Teto declarado.** `kMaxIngestBytes` num único lugar do núcleo, com valor
   inicial de 256 MB. Acima do teto, `E_TOO_LARGE`, com o teto na mensagem.
2. **Leitura em blocos.** O ficheiro é lido em blocos de 1 MB, e o cancelamento é
   consultado entre blocos por `std::atomic<bool>` com `memory_order_relaxed`.
3. **Fora do thread de áudio e fora do thread de mensagem.** A leitura corre num
   thread próprio, criado por pedido e juntado no fim. Nenhuma alocação acontece
   no callback.
4. **Publicação atómica.** O material só é publicado na fila depois de a leitura
   terminar com sucesso. Cancelar não publica nada.
5. **Progresso.** O thread de leitura publica `bytesRead` num `std::atomic<size_t>`
   que a interface lê a 30 Hz, para a barra de progresso.
6. **Erro tipado preservado.** Os erros existentes (`E_BAD_MZ`, `E_BAD_PE`,
   `E_TRUNCATED`, `E_OOB`, `E_TOO_MANY_SECTIONS`) continuam a sair do parser sem
   alteração.
7. **Sem regressão.** Um ficheiro dentro do teto produz exatamente o mesmo
   `SourceInfo` que a leitura síncrona produzia.

## Critérios de aceite

- [ ] ficheiro de 1 KB, 1 MB, 100 MB e acima do teto: os três primeiros tocam, o
      último devolve `E_TOO_LARGE` com o limite na mensagem;
- [ ] cancelar a meio não troca o material nem deixa estado parcial;
- [ ] a leitura de 100 MB não bloqueia o thread de mensagem por mais de 16 ms
      (medido por contador de tempo na interface de teste);
- [ ] o guard de alocação continua verde durante a reprodução;
- [ ] nenhum teste existente muda de resultado.

## Não-objetivos

- **Leitura por memória mapeada.** Fica para depois; `std::ifstream` em blocos
  resolve o problema e não acrescenta dependência de plataforma.
- **Análise incremental em streaming para a entropia.** A F022 constrói a
  waveform numa passagem própria; a curva de entropia continua a ser calculada
  uma vez, no fim da leitura.
- **Retomar leitura interrompida.** Cancelar é cancelar.

## Testes

| Teste | O que prova |
| --- | --- |
| `Ingest.OversizeFileReturnsTooLargeWithLimit` | teto e mensagem |
| `Ingest.CancelStopsWithinOneChunk` | latência do cancelamento |
| `Ingest.CancelLeavesPreviousMaterialPlaying` | atomicidade da publicação |
| `Ingest.ProgressReachesTotalOnSuccess` | contador de progresso |
| `Ingest.SameSourceInfoAsSynchronousPath` | ausência de regressão |
