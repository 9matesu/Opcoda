# 11. BPMN do fluxo de dados

## Leitura do diagrama

Arquivo executável: `docs/opcoda-fluxo.bpmn`. Abre no bpmn.io (navegador, sem instalar) ou no Bizagi Modeler. O diagrama usa três pools: Usuário, Opcoda (com lanes UI, Parser e Motor DSP) e DAW/Host como caixa-preta.

O fluxo começa quando o usuário seleciona um `.exe` e termina em dois fins possíveis: áudio entregue ao mix bus ou arquivo rejeitado com erro tipado.

## Pools e lanes

- Usuário: seleciona o arquivo e recebe som ou mensagem de erro.
- Opcoda, lane UI: diálogo de arquivo, envio do job e exibição de erros.
- Opcoda, lane Parser: mapeamento somente-leitura, validação e cálculo de entropia. Roda fora da thread de áudio.
- Opcoda, lane Motor DSP: consumo do descritor, agendamento de grãos, leitura com janela, mixagem, DC-blocker e limiter. Roda na thread de áudio, sem chamadas ao SO.
- DAW/Host: recebe o áudio do callback `process()`.

## Elementos principais

| Elemento | Tipo BPMN | Papel |
| --- | --- | --- |
| Arquivo selecionado | evento de início com mensagem | chega do usuário com o caminho do binário |
| Abrir ou arrastar, Enviar ao parser | tarefas na lane UI | coleta e despacho sem bloquear o áudio |
| Mapear READONLY, Validar PE | tarefas na lane Parser | sandboxing e bounds checking |
| Válido? | gateway exclusivo | desvia para erro ou para cópia do buffer |
| Mostrar erro | tarefa na lane UI | exibe código como E_BAD_PE, sem exceção |
| Copiar + entropia, Fila SPSC | tarefas na lane Parser | cópia defensiva e fila lock-free |
| Ler descritor, Agendar grãos, Ler grãos, Mixar + limiter | tarefas na lane DSP | núcleo granular em tempo real rígido |
| Áudio entregue | evento de fim com mensagem | leva o estéreo ao DAW |
| Arquivo rejeitado | evento de fim simples | encerra o ramo de erro |

## Cores

UI em azul, Parser em âmbar, DSP em verde. Gateway em laranja, ramo de erro em vermelho e fluxos de mensagem em azul. Rótulos curtos, de no máximo 3 palavras, para nenhum texto estourar a caixa.

## Correspondência com o pipeline

A fronteira entre as lanes Parser e Motor DSP é a fila SPSC do documento de arquitetura (`06-metodologia-arquitetura.md`). Nenhuma seta cruza do Parser para o DSP sem passar pela tarefa Publicar, e o gateway Binário válido cobre os códigos de erro da mitigação de falhas. A telemetria reversa para a GUI segue o mesmo canal lock-free no sentido contrário. O controle MIDI (learn e CC) chega ao DSP como parâmetro atômico pela mesma via, sem passar pelo parser. A tarefa de abertura cobre diálogo de arquivo e drag-and-drop do SO.
