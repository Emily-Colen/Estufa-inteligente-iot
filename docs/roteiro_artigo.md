# Roteiro para artigo de acordo com o modelo INIC/FEAU 2025 enviado

**Título provisório:** DESENVOLVIMENTO DE SISTEMA IOT PARA CLIMATIZAÇÃO E IRRIGAÇÃO AUTOMATIZADA DE ESTUFAS

**Resumo (rascunho para trabalho em desenvolvimento):** Este trabalho propõe o desenvolvimento de um sistema de monitoramento e controle automatizado de climatização e irrigação de estufas utilizando a plataforma ESP32. São empregados sensores de temperatura, umidade relativa do ar, umidade do solo e nível mínimo de reservatório. A placa executa algoritmos locais de atuação com limites de segurança e transmite leituras pela rede Wi-Fi a um serviço web. O servidor disponibiliza interface de acompanhamento e armazena as medições em um banco de dados SQLite. Também são previstos comandos remotos para alteração de parâmetros e operação manual. A validação experimental envolverá testes de acionamento, reconexão e preservação dos registros. Os resultados e a conclusão serão redigidos após as medições e os testes em bancada.

**Palavras-chave:** Internet das Coisas. Automação Agrícola. Estufas. Monitoramento Remoto. ESP32.

**Introdução:** contextualizar desafios de temperatura, umidade e irrigação na agricultura protegida; pesquisar bibliografia acadêmica atual; declarar objetivo geral de desenvolver e avaliar protótipo IoT acessível.

**Metodologia:** hardware (ESP32, DHT22, sensor capacitivo e boia); acionamento MOSFET; leitura a cada 5 s; controle por histerese; API REST; SQLite; interface web; ensaios com variação controlada das leituras; validação de acionamentos, reconexão e integridade de registros.

**Resultados:** inserir tabela real contendo precisão, periodicidade, percentuais de sucesso de transmissão, latência de comando, tempo de reação, consumo de água (somente com medidor apropriado), registros e gráficos extraídos da execução.

**Discussão:** analisar sensibilidade à calibração, limites do DHT22, atrasos de rede, operação offline e riscos de falha de sensor; comparar com literatura usando citações verificáveis.

**Conclusão:** escrever somente após executar o protótipo e medir resultados, destacando requisitos atendidos e limitações.

**Referências:** preparar de acordo com NBR 6023/2018 e citações conforme NBR 10520/2023, conforme modelo recebido. Não inserir bibliografia fictícia.

**Importante:** o modelo anexado informa extensão entre 4 e 6 páginas, Arial 10 para corpo e estrutura específica. Este roteiro é material de apoio, **não é um artigo validado ou submetido**.
