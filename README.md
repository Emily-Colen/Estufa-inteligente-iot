# ESTUFA IoT — climatização e irrigação automática

Protótipo acadêmico completo com **ESP32 + servidor FastAPI + banco SQLite + dashboard web**. Inclui simulação sem hardware.

## Arquitetura

```
DHT22 (temperatura/UR) --┐
Sensor umidade solo -----+--> ESP32 --> Wi-Fi --> HTTPS --> FastAPI --> SQLite
Boia nível água ---------┘      |                           |
                               |                        Dashboard web
                        Driver MOSFET bomba               comandos
                        Driver MOSFET fan                     |
                               +<------ consulta HTTPS -------+
```

O ESP32 executa o controle **localmente**, mesmo sem internet. O servidor registra medições e disponibiliza comandos; o ESP32 consulta a configuração a cada 10 s. A primeira implantação utiliza HTTP somente para **teste em rede LAN confiável**. Para implantação remota, hospede o serviço sob **HTTPS com certificado válido**, configure `USE_LOCAL_HTTP=false`, URL e CA raiz no firmware e não exponha a porta HTTP à Internet.

## Material

- ESP32 DevKit V1 (alimentação USB 5 V)
- DHT22 (temperatura e umidade do ar)
- Sensor capacitivo de umidade do solo com saída analógica **0–3,3 V**
- Chave boia/sensor de nível com contato seco, normalmente usada no modo que aterra o GPIO quando seguro
- Ventoinha DC 12 V + bomba de irrigação DC 12 V de baixa potência
- 2 módulos de acionamento MOSFET **compatíveis com lógica 3,3 V**, dimensionados para corrente de partida; proteções contra transientes/carga indutiva
- Fonte 12 V apropriada; terra comum para circuitos de baixa tensão quando exigido pelo driver; jumpers, tubulação, tanque
- Resistor pull-up 10 kΩ no DHT22 sem módulo e conexões isoladas contra água

**Segurança:** ESP32 não alimenta bomba/ventoinha diretamente. Verifique a tensão da fonte, pinagem, polaridade, correntes e instalação dos drivers. Use somente cargas DC de baixa tensão neste protótipo. Não conecte rede elétrica 127/220 V à protoboard. Sensor de nível falhando deve parar a bomba. Adicione um fusível dimensionado e proteção de retorno indutivo no acionamento.

## Ligações

| Origem | Pino ESP32 | Observação |
|---|---|---|
| DHT22 DATA | GPIO4 | VCC 3V3 e GND, pull-up se necessário |
| Umidade do solo AO | GPIO34 | alimentar módulo em 3,3 V, saída <= 3,3 V |
| Chave boia | GPIO27 | outra ponta em GND, nível seguro = LOW |
| Entrada driver ventilação | GPIO25 | saídas ativas em HIGH; conferir módulo |
| Entrada driver bomba | GPIO26 | saídas ativas em HIGH; conferir módulo |
| Alimentação de motores | fonte 12V | dimensionar corrente/segurança |

**Nota:** a luminosidade e volume de água **não são medidos nesta versão**. A interface mostra luminosidade como “—”. O nível do reservatório é apenas seguro/baixo, não uma porcentagem. Não há medição de consumo de água sem sensor de vazão.

## Rodar o servidor (Windows PowerShell)

```powershell
cd server
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
$env:DEVICE_KEY = "troque-por-uma-chave-longa-esp32"
$env:ADMIN_KEY = "troque-por-outra-chave-longa-dashboard"
uvicorn app:app --host 0.0.0.0 --port 8000
```

Abra `http://localhost:8000` e pressione **Credencial** para informar `ADMIN_KEY`. Encontre o IP local do computador via `ipconfig` e configure `SERVER_URL` no `.ino` como `http://IP_DO_COMPUTADOR:8000`. Permita acesso à porta 8000 no firewall apenas pela rede local confiável. Para testar sem hardware, em outro terminal execute:

```powershell
cd server
$env:DEVICE_KEY = "troque-por-uma-chave-longa-esp32"
python simulator.py
```

O simulador produz valores fictícios e **não** controla atuadores reais.

## ESP32 com Arduino IDE

1. Instale a plataforma `esp32 by Espressif Systems` no Gerenciador de Placas.
2. Instale `DHT sensor library`, `Adafruit Unified Sensor` e `ArduinoJson` versão 7.
3. Abra `firmware/EstufaIoT/EstufaIoT.ino`.
4. Configure SSID, senha, chave do dispositivo, IP do servidor e calibração do solo.
5. Selecione o modelo da sua placa ESP32, compile e envie. Monitore a serial em 115200 baud.
6. Verifique **temperatura, umidade, solo, reservatório e eventos** na interface.

**Calibração obrigatória:** com sensor seco, leia ADC e atualize `SOIL_DRY`. Em solo úmido representativo, leia ADC e atualize `SOIL_WET`. Não use o sensor como instrumento de precisão sem calibração apropriada. Os limites padrões de 35% e 55% são experimentais, não agronômicos.

## Regras implementadas

- Ventilação automática liga em `temperatura >= alvo + 1°C` e desliga em `temperatura <= alvo - 0,5°C`.
- Bomba inicia quando `solo <= solo_min` e nível está seguro; desliga quando `solo >= soil_stop`.
- Bomba é desligada após **12 s** de funcionamento contínuo e possui **30 s de intervalo mínimo** após desligar.
- Em modo manual o usuário solicita estados de fan e bomba; proteções da bomba permanecem ativas.
- Falha do DHT22 desliga fan/bomba. Falha/nível baixo da boia desliga bomba.
- Retentativas Wi-Fi sem bloquear intencionalmente o ciclo principal. Chamadas HTTP possuem timeout de 3,5 s.

**Limitações conhecidas:** Sem armazenamento offline persistente no ESP32; ao perder internet, o controle local permanece mas amostras não são preservadas para reenvio. A saída para os atuadores não possui realimentação física; “ligada” indica comando do ESP32 e não comprova fluxo. Os comandos remotos chegam por *polling* de 10 s. A versão de bancada depende de configurações e deve ser validada em hardware. Em produção, acrescente watchdog/failsafe externo e tratamento de reconexão mais robusto, autenticação de usuário e autorização por dispositivo, proteção elétrica, bateria/UPS conforme aplicação e TLS na rede externa.

## API e armazenamento

Banco `server/estufa.db` criado automaticamente. Tabelas:

- `telemetry`: instante UTC, sensor, saída fan, saída bomba, modo, alerta.
- `settings`: modo e setpoints editáveis com versão incremental.
- `events`: alterações detectadas e configuração.

| Método | Rota | Credencial | Uso |
|---|---|---|---|
| POST | `/api/device/telemetry` | `X-Device-Key` | ESP32 envia JSON |
| GET | `/api/device/settings?device_id=estufa-01` | `X-Device-Key` | ESP32 recebe configurações |
| GET | `/api/dashboard` | `X-Admin-Key` | painel consulta históricos e configurações |
| PUT | `/api/settings` | `X-Admin-Key` | painel altera modo e setpoints |
| GET | `/health` | nenhuma | saúde do serviço |

Exemplo de medição:

```json
{"device_id":"estufa-01","temperature":28.4,"humidity":67,"soil":52,"light":null,"tank_ok":true,"fan":true,"pump":false,"mode":"auto","alert":"NORMAL"}
```

## Cobertura dos requisitos

- Coleta de dados: DHT22, umidade do solo, chave boia.
- Processamento: ESP32 classifica leituras e aplica histerese.
- Wi-Fi: ESP32 conectado à rede local.
- Armazenamento: SQLite persistente em servidor; serviço pode ser hospedado em VM na nuvem.
- Visualização: dashboard responsivo com ilustração e gráficos atualizados a cada 5 s.
- Atuação remota: parâmetros e estado manual de bomba e ventilação.
- Escalabilidade: API com `device_id`; interface inicialmente exibe `estufa-01`.
- Confiabilidade: reconexão Wi-Fi, controle autônomo sem internet e limites de proteção.
- Segurança: duas chaves de API independentes; **TLS somente quando configurado em servidor HTTPS**, não no exemplo local HTTP.
- Eficiência: leituras a cada 5 s e comandos a cada 10 s; alimentação fixa por USB, sem deep sleep (não indicado para controle contínuo).

## Cenários de testes

1. Sem sensores: bombas e ventilação ficam desligadas, mostra falha de leitura.
2. Aumentar temperatura além do alvo+1: ventilação liga; abaixo de alvo-0,5 desliga.
3. Reduzir umidade de solo abaixo do mínimo: bomba liga se boia indicar nível seguro.
4. Abrir a boia: bomba desliga imediatamente **na próxima iteração da leitura** (até 5 s); para proteção imediata em produção, ligar boia de segurança também em intertravamento elétrico.
5. Forçar sensor inválido: saída desliga.
6. Derrubar Wi-Fi: automação local continua; retorno recupera dashboard.
7. No painel, mudar modo e setpoints: firmware recebe em até cerca de 10 s + latência.

## Nota sobre artigo acadêmico anexado

O modelo INIC/FEAU 2025 exige Resumo (7–12 linhas), 3–5 palavras-chave, Introdução, Metodologia, Resultados, Discussão, Conclusão e Referências. Este repositório entrega **implementação e roteiro experimental**; resultados reais só devem ser redigidos após testes, não devem ser inventados. Consulte `docs/roteiro_artigo.md`.
