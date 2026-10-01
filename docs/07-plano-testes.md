# 7. Plano de testes e critérios de validação

## T1. Atenuação de DC e ruído residual

Método: três binários de referência no MVP, incluindo notepad.exe, um binário comprimido com UPX e um arquivo sintético zerado. Captura de 10 segundos com grão de 40 ms e densidade média. FFT com N 65536 e janela Blackman-Harris.

Critério: atenuação de ao menos 40 dB no bin DC em relação à leitura bruta e energia abaixo de 20 Hz menor que -60 dBFS.

## T2. Latência e CPU sob polifonia

Método: sessão no REAPER com buffers de 128, 256 e 512 samples a 44,1 e 48 kHz, em máquina de referência intermediária. Medição de tempo médio e p99 do process, mais carga de CPU do sistema. Casos com 1, 4 e 8 vozes e grãos de 10 e 100 ms.

Critério: p99 abaixo de 50% do orçamento (2,9 ms para 128 samples a 44,1 kHz) e zero xruns em 5 minutos contínuos. Pluginval sem falhas.

## T3. Estresse com binários corrompidos

Método: corpus enxuto no MVP com 50 mutações por bit-flip, truncamento e cabeçalhos forjados, mais 10 casos de borda como arquivo vazio, 1 byte e e_lfanew fora do arquivo. Execução em loop do parser com host de teste.

Critério: zero segfaults, zero leaks, 100% das entradas inválidas rejeitadas com erro tipado e cobertura de branch do parser acima de 70%.

## T4. Aceite de uso (arrasto, parâmetros e MIDI)

Método: arrastar um .exe para o standalone e para o VST3 no REAPER, mover os 6 parâmetros principais, mapear 2 CCs via MIDI learn e tocar 2 minutos em cada formato.

Critério: som sai em menos de 2 s após o arrasto, zero xruns e todo CC mapeado move o parâmetro correspondente.

Complemento qualitativo: teste de usabilidade com 5 usuários e escala SUS, mais escuta comparativa entre leitura bruta e saída tratada.
