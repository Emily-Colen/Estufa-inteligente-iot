
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <math.h>

constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_SOIL = 34;
constexpr uint8_t PIN_LIGHT = 35;
constexpr uint8_t PIN_TANK = 33;
constexpr uint8_t PIN_FAN = 26;
constexpr uint8_t PIN_PUMP = 27;
constexpr uint8_t PIN_GROWLIGHT = 2;

const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";
const char *API_HOST = "estufa-inteligente-iot.onrender.com";
const char *API_URL = "https://estufa-inteligente-iot.onrender.com/api/telemetry";
const char *DEVICE_KEY = "";
const char *DEVICE_ID = "estufa-01";

// Modo de demonstracao: nao utiliza arquivo de CA.

// LIMITES DIDATICOS
constexpr float TEMP_FAN_ON = 29.0f;
constexpr float TEMP_FAN_OFF = 27.0f;
constexpr float HUMIDITY_FAN_ON = 80.0f;
constexpr float HUMIDITY_FAN_OFF = 70.0f;
constexpr float SOIL_PUMP_ON = 40.0f;
constexpr float SOIL_PUMP_OFF = 55.0f;
constexpr float LIGHT_GROW_ON = 30.0f;
constexpr float LIGHT_GROW_OFF = 50.0f;

constexpr unsigned long SENSOR_PERIOD = 3000UL;
constexpr unsigned long POST_PERIOD = 15000UL;
constexpr unsigned long WIFI_RETRY = 10000UL;
constexpr unsigned long MAX_PUMP_MS = 10000UL;
constexpr unsigned long PUMP_COOLDOWN_MS = 20000UL;
constexpr unsigned long DHT_STALE_MS = 9000UL;

// Potenciometro Wokwi: 0 = molhado, 4095 = seco
constexpr int RAW_WET = 0;
constexpr int RAW_DRY = 4095;

DHT dht(PIN_DHT, DHT22);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledReady = false;

float tempC = NAN, airPct = NAN, soilPct = 0, lightPct = 0;
bool reservoirOK = false, sensorOK = false;
bool fanOn = false, pumpOn = false, growLightOn = false;
bool cooldown = false;
unsigned long lastSample = 0, lastPost = 0, lastWifiTry = 0;
unsigned long lastValidDht = 0, pumpStart = 0, pumpStopped = 0;
bool wifiAttempted = false;

void stopPump(unsigned long now)
{
  if (pumpOn)
  {
    pumpOn = false;
    pumpStopped = now;
    cooldown = true;
    digitalWrite(PIN_PUMP, LOW);
    Serial.println("[BOMBA] Desligada");
  }
}

void maintainWiFi(unsigned long now)
{
  if (WiFi.status() == WL_CONNECTED)
    return;
  if (wifiAttempted && now - lastWifiTry < WIFI_RETRY)
    return;
  wifiAttempted = true;
  lastWifiTry = now;
  Serial.println("[WIFI] Tentando conectar...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
}

void enforceSafety(unsigned long now)
{
  // Mesmo entre amostras, proteger bomba contra falta de agua e tempo maximo.
  if (pumpOn && (!reservoirOK || !sensorOK ||
                 digitalRead(PIN_TANK) != LOW || now - pumpStart >= MAX_PUMP_MS))
  {
    stopPump(now);
  }
  if (sensorOK && now - lastValidDht > DHT_STALE_MS)
  {
    sensorOK = false;
    fanOn = false;
    growLightOn = false;
    stopPump(now);
    digitalWrite(PIN_FAN, LOW);
    digitalWrite(PIN_GROWLIGHT, LOW);
    Serial.println("[ALERTA] DHT22 indisponivel por tempo prolongado");
  }
}

void sampleAndControl(unsigned long now)
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isfinite(t) && isfinite(h) && t >= -40 && t <= 80 && h >= 0 && h <= 100)
  {
    tempC = t;
    airPct = h;
    lastValidDht = now;
    sensorOK = true;
  }
  else if (!sensorOK || now - lastValidDht > DHT_STALE_MS)
  {
    sensorOK = false;
  }

  reservoirOK = digitalRead(PIN_TANK) == LOW;
  long soilSum = 0, lightSum = 0;
  for (int i = 0; i < 8; ++i)
  {
    soilSum += analogRead(PIN_SOIL);
    lightSum += analogRead(PIN_LIGHT);
  }
  int soilRaw = soilSum / 8;
  int lightRaw = lightSum / 8;
  soilPct = constrain(100.0f * (RAW_DRY - soilRaw) / (RAW_DRY - RAW_WET), 0.0f, 100.0f);
  // LDR no Wokwi: leitura ADC alta significa menor iluminacao nesta ligacao.
  // Inverter para que luminosity (%) aumente quando o ambiente fica mais claro.
  lightPct = constrain(100.0f * (4095 - lightRaw) / 4095.0f, 0.0f, 100.0f);

  if (!sensorOK)
  {
    fanOn = false;
    growLightOn = false;
    stopPump(now);
  }
  else
  {
    if (tempC >= TEMP_FAN_ON || airPct >= HUMIDITY_FAN_ON)
      fanOn = true;
    else if (tempC <= TEMP_FAN_OFF && airPct <= HUMIDITY_FAN_OFF)
      fanOn = false;

    if (lightPct < LIGHT_GROW_ON)
      growLightOn = true;
    else if (lightPct > LIGHT_GROW_OFF)
      growLightOn = false;

    if (pumpOn && (soilPct >= SOIL_PUMP_OFF || !reservoirOK || now - pumpStart >= MAX_PUMP_MS))
    {
      stopPump(now);
    }
    if (cooldown && now - pumpStopped >= PUMP_COOLDOWN_MS)
      cooldown = false;
    if (!pumpOn && !cooldown && reservoirOK && soilPct <= SOIL_PUMP_ON)
    {
      pumpOn = true;
      pumpStart = now;
      Serial.println("[BOMBA] Irrigacao iniciada");
    }
  }

  digitalWrite(PIN_FAN, fanOn ? HIGH : LOW);
  digitalWrite(PIN_PUMP, pumpOn ? HIGH : LOW);
  digitalWrite(PIN_GROWLIGHT, growLightOn ? HIGH : LOW);
  Serial.printf("[ESTUFA] T=%.1f C Ar=%.1f%% Solo=%.1f%% Luz=%.1f%% Agua=%s Fan=%s Bomba=%s Cultivo=%s\n",
                tempC, airPct, soilPct, lightPct, reservoirOK ? "SIM" : "NAO",
                fanOn ? "ON" : "OFF", pumpOn ? "ON" : "OFF", growLightOn ? "ON" : "OFF");

  if (oledReady)
  {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.setTextSize(1);
    oled.println("ESTUFA INTELIGENTE");
    if (sensorOK)
      oled.printf("Temp: %.1f C\nAr: %.1f %%\n", tempC, airPct);
    else
      oled.println("DHT: FALHA");
    oled.printf("Solo: %.0f %% Luz: %.0f %%\n", soilPct, lightPct);
    oled.printf("Agua: %s\n", reservoirOK ? "SIM" : "NAO");
    oled.printf("F:%d B:%d L:%d\n", fanOn, pumpOn, growLightOn);
    oled.display();
  }
}

// Envio HTTPS isolado: nao executar outros handshakes TLS no setup.
// Uma falha na rede nunca deve alterar a decisao automatica da estufa.
void imprimirErroTLS(WiFiClientSecure &tls, int httpCode)
{
  char mensagem[160] = {0};
  int tlsCode = tls.lastError(mensagem, sizeof(mensagem));
  Serial.printf("[API] HTTP=%d WiFi=%d heap=%u\n", httpCode,
                WiFi.status(), (unsigned)ESP.getFreeHeap());
  Serial.printf("[API] TLS=%d %s\n", tlsCode, mensagem);
  if (tlsCode == -29312)
  {
    Serial.println("[API] EOF no handshake TLS; verificar gateway Wokwi/cadeia TLS.");
  }
}

void sendTelemetry()
{
  if (!sensorOK)
  {
    Serial.println("[API] Sem envio: DHT22 invalido");
    return;
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[API] Sem envio: Wi-Fi desconectado");
    return;
  }
  if (!strlen(DEVICE_KEY))
  {
    Serial.println("[API] Falta DEVICE_KEY de teste");
    return;
  }
  if (strncmp(API_URL, "https://", 8) != 0)
  {
    Serial.println("[API] URL deve iniciar com https://");
    return;
  }
  if (time(nullptr) < 1700000000)
  {
    Serial.println("[API] Aguardando sincronizacao NTP");
    return;
  }
  WiFiClientSecure tls;
  // MODO ACADEMICO: criptografa, mas nao autentica o servidor.
  // Nunca usar com credenciais de producao ou hardware real.
  tls.setInsecure();
  tls.setTimeout(8000);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setReuse(false);
  if (!http.begin(tls, API_URL))
  {
    Serial.println("[API] http.begin() falhou");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);
  char data[400];
  // luminosity: percentual estimado de iluminacao; NAO corresponde a lux.
  int n = snprintf(data, sizeof(data),
                   "{\"device_id\":\"%s\",\"temperature\":%.2f,\"air_humidity\":%.2f,"
                   "\"soil_moisture\":%.2f,\"luminosity\":%.2f,\"reservoir_ok\":%s,"
                   "\"fan_on\":%s,\"pump_on\":%s}",
                   DEVICE_ID, tempC, airPct, soilPct, lightPct,
                   reservoirOK ? "true" : "false", fanOn ? "true" : "false", pumpOn ? "true" : "false");
  if (n <= 0 || n >= (int)sizeof(data))
  {
    Serial.println("[API] Erro: payload excedeu buffer");
    http.end();
    return;
  }
  Serial.println("[API] Enviando telemetria HTTPS...");
  int status = http.POST((uint8_t *)data, (size_t)n);
  if (status == 201)
  {
    Serial.println("[API] HTTP 201: medicao gravada no Supabase");
  }
  else if (status > 0)
  {
    Serial.printf("[API] HTTP %d: %s\n", status, http.getString().c_str());
  }
  else
  {
    Serial.printf("[API] Falha HTTP %d: %s\n", status,
                  http.errorToString(status).c_str());
    imprimirErroTLS(tls, status);
  }
  http.end();
}

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_TANK, INPUT_PULLUP);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_GROWLIGHT, OUTPUT);
  digitalWrite(PIN_FAN, LOW);
  digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_GROWLIGHT, LOW);
  analogReadResolution(12);
  dht.begin();
  Wire.begin(21, 22);
  oledReady = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledReady)
  {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.display();
  }
  WiFi.mode(WIFI_STA);
  maintainWiFi(millis());
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("=== ESTUFA INTELIGENTE - DEMONSTRACAO WOKWI ===");
  Serial.println("Sensores/atuadores locais ativos mesmo sem nuvem");
  Serial.println("[TLS] Modo demonstracao: validacao de CA desativada");
}

void loop()
{
  unsigned long now = millis();
  maintainWiFi(now);
  enforceSafety(now);
  if (now >= 2500 && (lastSample == 0 || now - lastSample >= SENSOR_PERIOD))
  {
    lastSample = now;
    sampleAndControl(now);
  }
  if (now >= 2500 && (lastPost == 0 || now - lastPost >= POST_PERIOD))
  {
    lastPost = now;
    sendTelemetry();
  }
  delay(10);
}
