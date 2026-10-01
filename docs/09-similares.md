# 9. Análise de similares

## Método

Comparação funcional de três sistemas existentes com o TG da equipe (Opcoda). Critérios: fonte sonora aceita, síntese granular em tempo real, leitura de binários, tratamento de sinal e segurança do parser. Fontes: páginas oficiais dos fabricantes e manual do Audacity, acessados em outubro de 2026.

Os similares escolhidos cobrem as duas pontas do problema: Granulator II e Quanta 2 representam o estado da arte em síntese granular musical, e o Audacity representa a prática atual de databending.

## Similar 1: Granulator II (Robert Henke / Monolake)

Dispositivo Max for Live baseado em síntese granular quasi-síncrona, lançado em 2013 e distribuído gratuitamente pelo autor. Funciona dentro do Ableton Live com Max for Live. Aceita qualquer sample arrastado para o visor, com posição, tamanho de grão, spray, LFO, quatro formas de janela, filtros multimodo em série e até 32 vozes.

Pontos fortes: som de referência no gênero, engine aberta como patch Max (dá para estudar por dentro), custo zero, comunidade grande com exemplos.

Limites para este TG: só lê áudio convencional, então não existe ingestão de binários nem leitura de estrutura PE. Roda só no ecossistema Ableton/Max, sem formato VST3 ou CLAP. Não há cálculo de entropia nem cadeia de condicionamento para offset DC, porque a fonte já é áudio bem comportado.

## Similar 2: Audio Damage Quanta 2

Sintetizador granular comercial da Audio Damage, empresa independente ativa desde 2002. Distribuído nos formatos VST3, AU, AAX e CLAP para macOS, Windows e Linux, com versão para iOS. É um instrumento polifônico com importação de samples, modulação extensa e foco em performance.

Pontos fortes: roda em qualquer DAW moderna, inclui CLAP como o Opcoda, interface pensada para palco e estúdio, produto mantido com suporte.

Limites para este TG: a fonte continua sendo sample tradicional. Não há parser de executáveis, mapa de seções PE ou modulação guiada por entropia de bytes. O tratamento de entrada assume áudio válido, então robustez contra binários corrompidos não faz parte do escopo.

## Similar 3: Audacity, importação de dados brutos

O Audacity permite importar qualquer arquivo como áudio não comprimido (File, Import, Raw Data). O usuário escolhe codificação (PCM, ADPCM, float), ordem de bytes, canais, offset inicial, percentual do arquivo e taxa de amostragem. É a ferramenta padrão para databending rápido.

Pontos fortes: lê qualquer arquivo, inclusive .exe, sem custo e com diálogo simples de parâmetros. Serve bem para experimentação pontual.

Limites para este TG: importação offline, sem tempo real e sem polifonia. Não aplica janelamento granular, não calcula entropia e não trata offset DC. Não há validação de estrutura PE nem proteção contra travamento do host, porque o processamento acontece fora do caminho de áudio.

## Tabela comparativa

| Critério | Granulator II | Quanta 2 | Audacity raw | Opcoda (este TG) |
| --- | --- | --- | --- | --- |
| Fonte sonora | sample WAV/AIFF | sample importado | qualquer arquivo bruto | .exe, PE e binários arbitrários |
| Granular em tempo real | sim, até 32 vozes | sim, polifônico | não | sim, 8 vozes (MVP) |
| Janelamento de grãos | sim, 4 formas | sim | não | sim, Hann e Gaussiana |
| Parser estrutural PE | não | não | não | sim, com bounds checking |
| Modulação por entropia | não | não | não | sim, Shannon por janela e seção |
| Condicionamento DC/limiter | não se aplica | parcial | não | sim, DC-blocker e limiter |
| Segurança contra corrompidos | não se aplica | não se aplica | não | sim, sandbox READONLY e fuzzing |
| Formatos | Max for Live | VST3, AU, AAX, CLAP | editor offline | VST3 e CLAP |
| Custo | gratuito | comercial | gratuito | protótipo acadêmico |

## Síntese

Granulator II e Quanta 2 mostram como um granular deve soar e se comportar num instrumento sério. O Audacity mostra como o databending acontece hoje: rápido, mas bruto e fora de tempo real. O espaço do Opcoda fica exatamente entre eles. Nenhum dos três lê estrutura PE com segurança nem usa entropia como sinal de controle, que são as duas contribuições do trabalho.
