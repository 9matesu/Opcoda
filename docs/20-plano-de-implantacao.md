1 Plano de implantação em produção

1.1 Recorte e decisão de modelo

Traça-se, neste documento, o plano de implantação do Opcoda em produção a partir do material já construído: um instrumento granular distribuído como *plugin* VST3 e aplicativo *standalone* para Windows, macOS e Linux, acompanhado de um site estático no repositório `9matesu/bloodwormtrax`. A decisão que baliza todo o restante é a adoção da venda direta proprietária, nos moldes dos fabricantes de instrumentos digitais, em que o produtor mantém sob seu controle a vitrine, o checkout, a conta do cliente, a emissão da chave e a distribuição dos binários. Afasta-se a intermediação de lojas: o instrumento não é aplicativo móvel, e o *host* de uma estação de trabalho digital não admite pacotes iOS ou Android. Submeter o Opcoda à App Store ou ao Google Play seria equívoco técnico e despesa sem contrapartida.

O EULA já versionado em `LICENSE.txt` assenta essa postura: a licença é pessoal, não exclusiva e intransferível, reserva ao autor a propriedade intelectual do software e autoriza a exportação de áudio sem pagamento de *royalties*. Nesse escopo, a venda não transfere código, franqueia o uso e materializa um contrato entre selo e músico. A arquitetura de implantação deve proteger essa relação: reduzir ao mínimo os pontos de terceirização, concentrar no selo a emissão da credencial e manter a cadeia de distribuição auditável por tag de versão.

Convenciona-se que a operação nasce com custo fixo anual único, referente somente ao nome de domínio. Hospedagem, certificado, integração contínua, armazenamento dos binários, DNS e observabilidade são absorvidos por camadas gratuitas cujos termos foram confrontados com o uso comercial pretendido. Onde a camada gratuita restringe esse uso, ela é descartada — foi o caso do plano Hobby da Vercel, reservado a uso pessoal e não comercial, portanto imprestável para um site que vende licenças.

1.2 Hospedagem do site

O site do selo é estático: `index.html`, `opcoda.html` e `sistema.html`, acompanhados de `css/`, `js/` e `assets/`, sem etapa de construção e sem código de servidor. Diante dessa materialidade, contratar servidor físico ou máquina virtual seria queima de capital: não há processo a manter, não há banco a migrar e não há estado de sessão a preservar. GitHub Pages resolve a necessidade sem contrapartida financeira, aproveitando o mesmo provedor que já hospeda o código e executa a integração contínua.

A publicação dá-se com a escolha do branch `master` e da pasta raiz nas configurações do repositório `bloodwormtrax`, gerando o endereço `https://9matesu.github.io/bloodwormtrax/`. As referências relativas a folhas de estilo, scripts e imagens já funcionam nesse caminho; importa conferir apenas os destinos absolutos e as rotas por *hash* após a troca de domínio, tarefa que se resolve com uma varredura local e uma passada de validação de links. A disciplina de manter o repositório público preserva a elegibilidade ao plano gratuito e permite que o histórico Git funcione como registro das alterações do site.

A crítica à alternativa Vercel não é de ordem técnica, mas jurídica: o plano Hobby, embora generoso em transferência e requisições, restringe o uso a projetos pessoais e não comerciais. Como a página `opcoda.html` oferece licença paga, enquadrar o site nessa categoria seria operar fora dos termos. Caso se exija Vercel por imposição externa, a migração para o plano Pro, de US$ 20 mensais, entra no orçamento; na ausência dessa imposição, GitHub Pages permanece como escolha.

1.3 Nome de domínio e resolução

O endereço `9matesu.github.io/bloodwormtrax` carece de autoridade comercial: carrega o nome de um usuário e um sufixo de provedor. Registra-se, por isso, `bloodwormtr.ax`, domínio curto e alinhado ao nome do selo. O arquivo `CNAME` na raiz do repositório fixa esse domínio junto ao GitHub Pages. A TLD `.ax` tem preço mais alto que `.com`: a pesquisa de registradores aponta valores entre US$ 31 e US$ 45 anuais nas opções mais baratas, com renovação no mesmo patamar; some-se a isso a exigência de titular elegível da Åland. Registra-se a ressalva, pois o valor real depende do registrador e da elegibilidade [DADO NECESSÁRIO: cotação fechada e confirmação de disponibilidade no registrador escolhido].

A resolução dá-se no Cloudflare DNS: `www` como CNAME apontando para `9matesu.github.io`; o ápice como quatro registros A para 185.199.108.153, 185.199.109.153, 185.199.110.153 e 185.199.111.153. O proxy laranja deve ser desligado durante a configuração inicial, pois o GitHub Pages emite e renova o certificado por conta própria e a interposição do proxy pode obstruir a validação. Concluída a propagação, o campo "Custom domain" do Pages recebe `bloodwormtr.ax` e o redirecionamento do `www` é ativado, de sorte que as duas formas convoquem o mesmo conteúdo.

1.4 Segurança, transporte e conservação

O certificado é emitido pelo Let's Encrypt por intermédio do GitHub Pages, sem emissão própria, sem renovação manual e sem custo. Basta marcar "Enforce HTTPS" para que toda requisição HTTP seja redirecionada; a checagem posterior cobre ausência de conteúdo misto, já que as imagens do site são carregadas por caminhos relativos e não por URLs inseguras. DNSSEC e proteção básica contra negação distribuída de serviço vêm incluídos na conta gratuita do Cloudflare, suficientes para um site estático que não processa dados de cartão.

Não há, nesta fase, informação sensível residindo no navegador: o `localStorage` guarda apenas tema, disposição, granularidade e moeda preferida, escolha correta porque nenhuma credencial deve ser confiada ao cliente. A aceitação de pagamentos recai sobre Stripe ou Mercado Pago, que assumem a conformidade PCI e tokenizam o cartão; o projeto não recebe, registra nem retransmite número de cartão. Quando o licenciamento evoluir, a chave será gerada no servidor de checkout e entregue por e-mail, nunca depositada em armazenamento local.

O backup do site é o próprio repositório Git, com histórico integral e remoto adicional sem custo — um espelho em segundo provedor basta para cobrir a perda de uma conta. Os binários encontram conservação equivalente nas Releases do GitHub, imutáveis por tag, de modo que a versão 0.1.1 continue acessível após a publicação da 0.2.0. Habilitam-se, por fim, Dependabot e varredura de segredos nas configurações de segurança do repositório público.

1.5 Distribuição do instrumento

A esteira de publicação em `.github/workflows/release.yml` é o carro-chefe da operação: ao receber uma tag `v*`, ela compila Windows, macOS e Linux, produz o instalador Inno Setup, os arquivos `Opcoda-<versão>-Windows-VST3.zip`, `Opcoda-<versão>-Windows-Standalone.zip`, o `.dmg` e o `.tar.gz`, e publica o conjunto como Release com notas geradas automaticamente. Não há servidor de download a manter, pois o GitHub exerce essa função sem cobrança e com disponibilidade satisfatória para o volume inicial do selo.

A fragilidade não está na esteira, mas na sequência de publicação. Conforme registrado em `AGENTS.md`, o commit de incremento de versão não pode conter `[skip ci]`, pena de o *push* da tag usar essa mensagem e o GitHub Actions saltar o gatilho `Release`. O histórico recente traz commits com essa marcação; impõe-se conferir o estado antes de etiquetar a próxima versão e validar, em uma tag de ensaio, se os quatro artefatos aparecem na página de publicação.

1.6 Preços e checkout

A política de preços adota valores de produto, não de conversão cambial: R$ 97 no Brasil, US$ 59 nos Estados Unidos e EUR 55 na Europa. Com a cotação de US$ 1 ≈ R$ 5,85 observada em 24 de novembro de 2026, os R$ 97 correspondem a cerca de US$ 16,5; o comprador brasileiro paga menos de um terço do valor cobrado no exterior, ao passo que o exterior paga o preço internacional cheio, sem desconto por arbitragem. A moeda é escolhida no topo da página `opcoda.html` e lembrada no navegador; os três botões de compra e todos os valores exibidos mudam juntos.

Os preços e os links de pagamento ficam concentrados em `js/checkout.js` do repositório `bloodwormtrax`. Essa decisão reduz a manutenção a um arquivo: trocar de provedor ou corrigir um link não exige tocar no HTML. O checkout é hosted, de Stripe ou Mercado Pago, integrado por link, sem servidor próprio na primeira fase. O custo é variável, da ordem de 2,9% mais US$ 0,30 no Stripe e de aproximadamente 3% a 6% no Mercado Pago conforme a modalidade, com mensalidade zero. Ao confirmar o pagamento, o comprador recebe por e-mail o link da Release correspondente, a chave de ativação e as instruções para as duas máquinas previstas no EULA e na página comercial.

A chave adota formato determinístico, por exemplo `BTX-XXXX-XXXX-XXXX`, gerada por HMAC com segredo mantido nos Segredos do GitHub Actions. A validação ocorre no próprio instrumento, aproveitando o padrão já assentado no núcleo C++20: leitura delimitada, erros tipados e nenhuma exceção atravessando a ABI do *plugin*. Isso preserva a restrição arquitetural de que o retorno de áudio não aloca, não bloqueia, não realiza E/S e não lança exceções; a checagem de licença dá-se na inicialização, fora da *thread* de áudio.

A ausência de backend impede revogação imediata e identificação de máquinas em tempo real. Registra-se essa limitação como estágio transitório: a conta de usuário, com histórico de compras e reemissão de chave, congrega o próximo incremento e exige a migração do site para um plano pago de hospedagem ou um servidor de aplicação. Nada disso é pré-requisito para faturar a primeira licença.

1.7 Correio eletrônico e comunicação

O endereço `bloodwormtrax@gmail.com`, já presente no rodapé das três páginas, permanece como identidade de atendimento. A profissionalização dá-se por Cloudflare Email Routing, que redireciona mensagens enviadas a `contato@bloodwormtr.ax` para a caixa existente sem custo. Para o envio automático da chave, o Resend oferece três mil mensagens mensais gratuitas, volume suficiente para o lançamento; em alternativa, pode-se usar a conta Google com senha de aplicativo acionada pelo Actions, observando os limites diários do serviço.

A comunicação comercial não depende de ferramenta de automação neste momento. A página de perguntas já centraliza as dúvidas recorrentes e o tempo de resposta de um dia útil nela declarado deve ser honrado; é esse compromisso, e não um pipeline de marketing, que sustenta a reputação do selo junto aos primeiros compradores.

1.8 Observabilidade e verificação

Cloudflare Web Analytics fornece métricas de página sem cookies, em conformidade com a postura de privacidade adotada no site. O UptimeRobot, no plano gratuito, monitora a disponibilidade do domínio e da página de licença em intervalos de cinco minutos. O GitHub Insights completa o quadro com histórico de commits, releases e consumo da esteira de integração. Nenhum desses serviços introduz custo ou dependência crítica: todos podem ser retirados sem afetar a venda.

A verificação final da implantação cobre, em ordem: resolução DNS nas duas formas do domínio; redirecionamento HTTP para HTTPS; carregamento das três páginas sem conteúdo misto; funcionamento da busca, dos filtros e das rotas por *hash*; troca de moeda e persistência da preferência; atualização dos três botões de compra; abertura do FAQ; e, sobretudo, a existência dos quatro artefatos na Release da versão anunciada. Um descolamento entre o preço exibido e o valor cobrado no checkout é falha de implantação, não detalhe cosmético.

1.9 Custos consolidados

O quadro discrimina o compromisso financeiro da operação. Os valores de comissão incidem por transação e não representam desembolso fixo.

| Item | Custo |
| --- | --- |
| Hospedagem do site — GitHub Pages | R$ 0 por mês |
| Domínio `bloodwormtr.ax` | US$ 31 a US$ 45 por ano [DADO NECESSÁRIO: registrador fechado] |
| Certificado TLS — Let's Encrypt via GitHub Pages | R$ 0 |
| DNS, DNSSEC e proteção básica — Cloudflare | R$ 0 |
| Integração contínua e publicação — GitHub Actions | R$ 0 dentro da cota gratuita |
| Armazenamento e distribuição dos binários — GitHub Releases | R$ 0 |
| Checkout — Stripe ou Mercado Pago | R$ 0 fixo, mais 3% a 6% por venda |
| E-mail transacional — Resend, até três mil mensagens | R$ 0 |
| Analytics e monitoramento | R$ 0 |
| Assinatura de código Windows e notarização macOS, posteriores | US$ 0 a US$ 500 por ano |

O desembolso para entrar em produção resume-se ao domínio, entre US$ 31 e US$ 45 anuais conforme o registrador. Todo o restante é absorvido por serviços cujos termos foram confrontados com a natureza comercial da operação; a única ressalva é a de que a hospedagem gratuita não serve de abrigo a um checkout embutido, motivo pelo qual o pagamento é terceirizado ao processador e o site permanece como vitrine e canal de entrega.

1.10 Sequência de execução

Registra-se o domínio; configuram-se os registros DNS e o domínio personalizado no GitHub Pages; marca-se o redirecionamento forçado para HTTPS; habilitam-se Dependabot, varredura de segredos e espelho do repositório. Em paralelo, confere-se o commit de versão quanto à ausência de `[skip ci]`, publica-se uma tag de ensaio e validam-se os artefatos da Release. A seguir, substituem-se os marcadores em `js/checkout.js` pelos links reais de pagamento, configura-se o Cloudflare Email Routing e o envio da chave pelo Resend. Por fim, instalam-se analytics e monitoramento e executa-se a lista de verificação do parágrafo anterior.

A ordem não é arbitrária: o domínio e o certificado vêm primeiro porque toda prova posterior depende de uma URL estável; a Release vem antes do checkout porque não se vende licença cujo binário não esteja publicado; o e-mail transacional vem por último porque só faz sentido após a existência de um comprador. Cada etapa, concluída, reduz uma incerteza concreta da operação.
