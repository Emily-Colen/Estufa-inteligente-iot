# Estufa Inteligente IoT 🌱

Sistema acadêmico de **monitoramento e automação de uma estufa**, desenvolvido com **ESP32**, simulação no **Wokwi**, API **FastAPI**, banco **PostgreSQL no Supabase** e dashboard web hospedado com o backend no **Render**.

O protótipo coleta informações ambientais, controla automaticamente ventilação, irrigação e iluminação e disponibiliza medições para acompanhamento pela internet.

> **Contexto:** Projeto Final da disciplina de Internet das Coisas (IoT), Engenharia da Computação — 2026. A implementação apresentada é uma **simulação acadêmica**, não um equipamento agrícola validado para operação física.

## Funcionalidades

- **Temperatura e umidade relativa do ar:** leitura pelo DHT22.
- **Umidade do solo:** leitura analógica simulada por potenciômetro.
- **Luminosidade:** leitura pelo LDR; valor convertido em índice percentual estimado (não em lux).
- **Reservatório:** chave digital que informa se há água disponível.
- **Ventilação automática:** acionamento de LED indicador conforme temperatura/umidade do ar, com histerese.
- **Irrigação automática:** acionamento de LED indicador com limite de duração, intervalo entre ciclos e bloqueio por falta de água.
- **Iluminação automática:** acionamento de LED indicador conforme o índice de luminosidade, com histerese.
- **Telemetria IoT:** envio de medições por Wi-Fi/HTTPS à API.
- **Histórico na nuvem:** armazenamento no Supabase e consulta pela API.
- **Dashboard VERDANT:** visualização de indicadores, condições da estufa, histórico e representação gráfica dos atuadores.

**Importante:** o status da iluminação mostrado no dashboard é **estimado a partir de `luminosity`**, pois o firmware/API atuais não transmitem explicitamente o estado `growLightOn`. Os estados de ventilação e irrigação são enviados na telemetria.

## Arquitetura

```text
              WOKWI — ESP32 DevKit V1
  DHT22 · Solo · LDR · Sensor de reservatório
                         |
               Processamento local
               Regras de automação
                         |
             LEDs: fan · pump · light
                         |
                Wi-Fi + HTTPS
                         |
                         v
              Render — API FastAPI
            POST /api/telemetry
                         |
                         v
             Supabase — PostgreSQL
           public.devices / telemetry
                         |
                         v
               FastAPI (consultas)
              latest / history
                         |
                         v
                Dashboard VERDANT
        Atualização periódica no navegador
```

O controle dos atuadores é **local no ESP32**: o dashboard acompanha o estado informado pelo dispositivo, mas não executa comandos remotos nesta versão.

## Tecnologias utilizadas

| Camada | Tecnologias |
|---|---|
| Microcontrolador | ESP32 DevKit V1, Arduino/C++ |
| Simulação | Wokwi |
| Sensores simulados | DHT22, potenciômetro (solo), LDR, chave de nível |
| Interface local | OLED SSD1306 e LEDs indicadores |
| Comunicação | Wi-Fi, JSON e HTTPS |
| Backend | Python, FastAPI, SQLAlchemy, psycopg |
| Banco de dados | PostgreSQL, Supabase |
| Hospedagem | Render |
| Interface web | HTML, CSS, JavaScript |

## Estrutura do repositório

```text
EstufaIoT/
├── firmware/
│   └── EstufaIoT/
│       └── EstufaIoT.ino
├── server/
│   ├── app.py
│   ├── database.py
│   ├── dashboard_routes.py
│   ├── dashboard_static/
│   │   ├── index.html
│   │   ├── style.css
│   │   └── app.js
│   ├── requirements.txt
│   └── simulator.py
├── docs/
│   └── roteiro_artigo.md
├── .gitignore
└── README.md
```

> A simulação em Wokwi pode conter arquivos próprios, como `diagram.json` e `libraries.txt`. Inclua-os no repositório se desejar distribuir o circuito completo, pois o arquivo `.ino` isolado não reproduz as conexões gráficas.

## Pinagem do ESP32

| Elemento | GPIO |
|---|---:|
| DHT22 (dados) | 4 |
| Umidade do solo (analógico) | 34 |
| LDR (analógico) | 35 |
| Reservatório / chave digital | 33 |
| LED de ventilação | 26 |
| LED de irrigação | 27 |
| LED de iluminação | 2 |
| OLED SDA / SCL | 21 / 22 |

**Observação:** os LEDs representam os atuadores durante a simulação. Não conecte bombas ou ventiladores diretamente aos GPIOs do ESP32 em uma implementação física.

## Regras de automação

Os limites abaixo são **valores didáticos configurados no firmware**, não recomendações agrícolas.

| Atuador | Liga quando | Desliga quando |
|---|---|---|
| Ventilação | Temperatura ≥ 29 °C **ou** umidade do ar ≥ 80% | Temperatura ≤ 27 °C **e** umidade do ar ≤ 70% |
| Irrigação | Solo ≤ 40%, reservatório com água e sem bloqueio temporário | Solo ≥ 55%, falta de água ou tempo máximo |
| Iluminação | Luminosidade < 30% | Luminosidade > 50% |

A iluminação conserva seu estado na faixa intermediária de 30% a 50% (**histerese**). A bomba possui tempo máximo de aproximadamente **10 segundos por ciclo** e espera de aproximadamente **20 segundos após o desligamento** antes de um novo acionamento, conforme o firmware testado.

O código também monitora a validade das leituras do DHT22 e utiliza condições de segurança para desligar atuadores quando necessário.

## Como executar

### 1. Simulação no Wokwi

1. Abra o circuito do projeto no **Wokwi**.
2. Utilize o firmware em `firmware/EstufaIoT/EstufaIoT.ino` (ou copie o código correspondente ao editor Wokwi).
3. Confira as bibliotecas utilizadas: `DHT sensor library`, `Adafruit GFX Library`, `Adafruit SSD1306` e suas dependências.
4. Configure uma chave **de teste** para o dispositivo em ambiente privado. **Nunca publique credenciais reais no repositório ou em simulações públicas.**
5. Inicie a simulação e acompanhe o **Monitor Serial**.
6. Altere temperatura, umidade do solo, luminosidade e estado do reservatório para observar os LEDs e a telemetria.

Quando o envio é aceito pela API, o Monitor Serial apresenta uma mensagem semelhante a:

```text
[API] HTTP 201: medicao gravada no Supabase
```

### 2. API local no VS Code

No terminal PowerShell, a partir da pasta `EstufaIoT`:

```powershell
cd .\server
python -m pip install -r requirements.txt
python -m uvicorn app:app --reload
```

O backend utiliza variáveis de ambiente para acessar o Supabase e validar as credenciais de API:

| Variável | Finalidade |
|---|---|
| `DATABASE_URL` | URI de conexão PostgreSQL do Supabase |
| `DEVICE_KEY` | Autenticação das medições enviadas pelo ESP32 |
| `ADMIN_KEY` | Autorização das consultas protegidas do dashboard |

Configure-as **localmente, sem versioná-las**. Um arquivo `server/.env` pode ser usado se o projeto estiver configurado para carregá-lo, como na implantação atual. Não envie o conteúdo desse arquivo ao GitHub.

Acesse:

- API: `http://127.0.0.1:8000/`
- Dashboard: `http://127.0.0.1:8000/dashboard`
- Documentação interativa: `http://127.0.0.1:8000/docs`

Na interface, utilize a opção **Acessar dados** com a `ADMIN_KEY` autorizada.

### 3. Implantação no Render

A implantação atual usa um **Web Service Python** com o diretório raiz configurado como `server`.

**Build command:**

```bash
pip install -r requirements.txt
```

**Start command:**

```bash
uvicorn app:app --host 0.0.0.0 --port $PORT
```

As variáveis `DATABASE_URL`, `DEVICE_KEY` e `ADMIN_KEY` devem ser configuradas no painel de ambiente do Render, **não no código-fonte**.

- [API publicada](https://estufa-inteligente-iot.onrender.com/)
- [Dashboard publicado](https://estufa-inteligente-iot.onrender.com/dashboard)

> No plano gratuito, o serviço pode suspender por inatividade e demorar a responder à primeira requisição.

## API e banco de dados

O backend utiliza tabelas no esquema `public` do Supabase, incluindo `devices`, `telemetry`, `settings`, `events` e `commands`. **Na versão apresentada, as operações demonstradas utilizam principalmente `devices` e `telemetry`**; não se deve interpretar a existência das demais tabelas como funcionalidade de controle remoto já implementada.

| Método | Endpoint | Descrição |
|---|---|---|
| `GET` | `/` | Status básico da API |
| `GET` | `/api/health/database` | Verificação da conexão com o banco |
| `GET` | `/api/tabelas` | Consulta auxiliar de tabelas |
| `POST` | `/api/telemetry` | Recebe medições (`X-Device-Key`) |
| `GET` | `/api/telemetry/latest?device_id=estufa-01` | Última medição (`X-Admin-Key`) |
| `GET` | `/api/telemetry/history?device_id=estufa-01&limit=50` | Histórico (`X-Admin-Key`) |
| `GET` | `/dashboard` | Abre a interface web |

Exemplo **ilustrativo** do JSON enviado pelo dispositivo:

```json
{
  "device_id": "estufa-01",
  "temperature": 25.0,
  "air_humidity": 61.0,
  "soil_moisture": 70.0,
  "luminosity": 20.0,
  "reservoir_ok": true,
  "fan_on": false,
  "pump_on": false
}
```

O campo `luminosity` representa um **percentual aproximado derivado do ADC do LDR**, e não uma medição calibrada em lux.

## Testes funcionais realizados

Os testes a seguir foram executados na simulação e confirmados durante o desenvolvimento:

- [x] Atualização de temperatura e umidade no dashboard.
- [x] Recuperação do indicador de dados recentes quando novas medições chegam.
- [x] Acionamento e desligamento automático da ventilação.
- [x] Acionamento da irrigação segundo a umidade do solo.
- [x] Bloqueio da bomba quando o reservatório está sem água.
- [x] Acionamento da iluminação conforme a luminosidade.
- [x] Gravação de medições no Supabase e consulta por meio da API.
- [x] Representação **estimada** da iluminação no dashboard usando a telemetria `luminosity`.

A validação em períodos prolongados, sob interrupções frequentes de rede ou em equipamentos físicos, ainda requer ensaios específicos.

## Limitações conhecidas e segurança

- **TLS acadêmico:** a comunicação HTTPS do firmware em Wokwi utiliza `setInsecure()`, que **não valida a identidade do servidor**. O tráfego é criptografado, mas essa configuração não oferece a autenticação exigida para uma implantação segura. Para uso real, utilizar verificação correta de certificados.
- **Intermitência de rede:** foram observadas falhas ocasionais de handshake TLS (`-29312`, EOF) no ambiente simulado, embora também tenham sido confirmados envios bem-sucedidos (`HTTP 201`).
- **Servidor gratuito:** o Render pode suspender por inatividade, aumentando a latência de algumas requisições.
- **Sem armazenamento offline persistente:** medições não entregues não são garantidamente recuperadas após falha de conexão.
- **Estado da iluminação:** o dashboard infere o acionamento a partir das leituras e da histerese; não recebe uma confirmação direta desse LED.
- **Eficiência energética:** o protótipo foi concebido para monitoramento contínuo e não inclui avaliação experimental de consumo ou modos de economia para bateria.
- **Simulação:** os LEDs indicam comandos do programa, não comprovam funcionamento de bomba, ventilador ou lâmpada físicos.
- **Controle remoto:** ainda não implementado; é opcional nos requisitos da disciplina.

**Nunca publique** `DATABASE_URL`, `DEVICE_KEY`, `ADMIN_KEY`, arquivos `.env` ou outras credenciais reais. Se alguma chave tiver sido exposta, substitua-a no serviço correspondente.

## Correspondência com os requisitos da disciplina

| Requisito | Evidência no projeto |
|---|---|
| Coleta de dados | DHT22, solo, LDR e reservatório |
| Processamento | Conversão e classificação de leituras no ESP32 |
| Conectividade | Wi-Fi do ESP32 simulado |
| Comunicação com a nuvem | FastAPI no Render e PostgreSQL no Supabase |
| Visualização em tempo real | Dashboard web com consultas periódicas |
| Atuação remota (opcional) | Não implementada nesta versão |
| Escalabilidade | API e registros organizados por `device_id`; expansão futura prevista |
| Confiabilidade | Automação local, lógica de reconexão e tratamento de falhas; limitações documentadas |
| Segurança | Uso de HTTPS e chaves distintas, com ressalva da validação TLS no Wokwi |
| Usabilidade | Indicadores, ilustração e histórico no dashboard |
| Eficiência energética | Considerada como aspecto de evolução, ainda não mensurada |


