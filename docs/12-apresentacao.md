# 12. Roteiro de apresentação em slides

Prompt pronto para colar no ChatGPT ou no Canva, seguido dos textos dos 8 slides. Apresentação simples, sem imagens, só título e tópicos curtos.

## Prompt

Crie uma apresentação simples de 8 slides, sem imagens, só título e até 4 tópicos curtos por slide, para alunos do 5º semestre de ADS. Tema: Opcoda, um sintetizador que transforma arquivos .exe em som por síntese granular. Tom acadêmico direto, frases curtas, sem jargão desnecessário. Slides: 1) título e equipe; 2) o problema; 3) o que é databending; 4) por que áudio bruto falha; 5) a solução Opcoda; 6) síntese granular e entropia; 7) arquitetura em tempo real; 8) entrega do MVP e próximos passos.

## Slide 1: título

- Opcoda: um sintetizador que transforma .exe em som
- TG, Análise e Desenvolvimento de Sistemas, 5º semestre
- Standalone e VST3 x64, entrega em 03/11

## Slide 2: o problema

- Arquivos .exe guardam bytes que podem virar som
- Abrir como áudio bruto gera estalos e ruído descontrolado
- Nenhuma ferramenta atual faz isso em tempo real com segurança

## Slide 3: o que é databending

- Ler um arquivo não sonoro como se fosse áudio
- Prática comum no Audacity, só que offline
- O resultado é imprevisível e difícil de controlar

## Slide 4: por que o áudio bruto falha

- Offset DC: bytes não têm média zero, o sinal desloca
- Transientes: cabeçalhos criam cliques sem valor musical
- Aliasing: variações rápidas demais dobram para o audível
- Risco: parser ingênuo lê fora dos limites e derruba o DAW

## Slide 5: a solução Opcoda

- Parser PE que lê seções com checagem de limites, só para leitura
- Fila lock-free entre interface e motor de áudio
- Grãos com janela, filtro DC-blocker e limiter na saída

## Slide 6: granular e entropia

- Síntese granular: nuvem de grãos de 1 a 100 ms (Roads, 2001)
- Entropia de Shannon mede a textura de cada trecho do binário
- Trecho denso vira timbre ruidoso, trecho homogêneo vira timbre estável

## Slide 7: tempo real de verdade

- Thread de áudio com orçamento de 2,9 ms por bloco
- 8 vozes pré-alocadas, nada de malloc ou mutex no loop
- MIDI learn e CC chegam como parâmetros atômicos

## Slide 8: MVP e próximos passos

- MVP em 03/11: arrastar .exe, 6 parâmetros, MIDI, sem xruns
- Depois: CLAP, bateria completa de testes e monografia
- Pergunta da banca: o tratamento granular reduz o DC em 40 dB?
