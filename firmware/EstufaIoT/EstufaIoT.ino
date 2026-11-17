/* Estufa IoT: prototipo academico ESP32, DHT22, sensor solo analogico, boia,
   ventoinha 12V e bomba 12V. HTTPS recomendado em implantacao.
   Bibliotecas: DHT sensor library (Adafruit), Adafruit Unified Sensor,
                ArduinoJson v7.
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SOIL_PIN 34
#define FLOAT_PIN 27
#define FAN_PIN 25
#define PUMP_PIN 26

// NAO acione motores diretamente pelos GPIOs. Use drivers e fonte separada.
const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASS = "SUA_SENHA";
const char* DEVICE_KEY = "dev-device-change-me";
const char* DEVICE_ID = "estufa-01";

// Para testes locais com servidor HTTP: USE_LOCAL_HTTP=true e IP do PC na LAN.
// Para producao: USE_LOCAL_HTTP=false, URL https e CA raiz valida.
const bool USE_LOCAL_HTTP = true;
const char* SERVER_URL = "http://192.168.1.100:8000";
const char* ROOT_CA = R"CA(
-----BEGIN CERTIFICATE-----
COLE_A_CA_RAIZ_DA_SUA_HOSPEDAGEM_HTTPS
-----END CERTIFICATE-----
)CA";

// Calibrar lendo valores ADC em solo seco e umido no prototipo.
const int SOIL_DRY = 3200;
const int SOIL_WET = 1400;
// Boia liga pino ao GND com reservatorio em nivel seguro.
const bool FLOAT_OK_LEVEL = LOW;
const unsigned long SAMPLE_MS=5000, SYNC_MS=10000;
const unsigned long PUMP_MAX_MS=12000, PUMP_COOLDOWN_MS=30000;
const unsigned long WIFI_RETRY_MS=10000;
DHT dht(DHT_PIN,DHT_TYPE);
float temp=NAN, hum=NAN, soil=NAN;
bool tankOk=false, fanOn=false, pumpOn=false, sensorsOk=false;
String mode="auto", alert="SEM_LEITURA";
float target=27, soilMin=35, soilStop=55;
bool manualFan=false, manualPump=false;
unsigned long lastSample=0,lastSync=0,lastWiFiRetry=0;
unsigned long pumpStarted=0,pumpStopped=0;
bool pumpEverStopped=false;

void setPump(bool on) {
  if(on==pumpOn) return;
  pumpOn=on;
  digitalWrite(PUMP_PIN,on?HIGH:LOW);
  if(on) pumpStarted=millis();
  else {pumpStopped=millis();pumpEverStopped=true;}
}
void setFan(bool on){fanOn=on;digitalWrite(FAN_PIN,on?HIGH:LOW);}

void controlLoop(){
  if(!tankOk || !sensorsOk){setPump(false);}
  if(!sensorsOk){setFan(false);alert="FALHA_SENSOR";return;}
  if(!tankOk) alert="RESERVATORIO_BAIXO";
  else if(temp>target+4)alert="TEMPERATURA_ALTA";
  else if(soil<soilMin)alert="SOLO_SECO";
  else alert="NORMAL";

  if(mode=="manual"){
    setFan(manualFan);
    if(!tankOk || !manualPump) setPump(false);
    else if(!pumpOn && (!pumpEverStopped || millis()-pumpStopped>=PUMP_COOLDOWN_MS)) setPump(true);
  } else {
    // Histerese: liga acima de alvo+1; desliga abaixo de alvo-0.5.
    if(temp>=target+1.0)setFan(true);
    else if(temp<=target-0.5)setFan(false);

    if(!tankOk || soil>=soilStop)setPump(false);
    else if(soil<=soilMin && !pumpOn &&
            (!pumpEverStopped || millis()-pumpStopped>=PUMP_COOLDOWN_MS))setPump(true);
  }
  // Limite de tempo: desligamento e espera mesmo se o solo continuar seco.
  if(pumpOn && millis()-pumpStarted>=PUMP_MAX_MS){setPump(false);alert="BOMBA_TEMPO_MAXIMO";}
}

void sample(){
  float t=dht.readTemperature(), h=dht.readHumidity();
  int adc=analogRead(SOIL_PIN);
  tankOk=(digitalRead(FLOAT_PIN)==FLOAT_OK_LEVEL);
  sensorsOk=isfinite(t)&&isfinite(h)&&t>=-10&&t<=65&&h>=0&&h<=100;
  if(sensorsOk){temp=t;hum=h;} // Mantem ultimo valor, mas sinaliza falha no status.
  soil=constrain(100.0f*(SOIL_DRY-adc)/(SOIL_DRY-SOIL_WET),0.0f,100.0f);
  controlLoop();
  Serial.printf("T %.1f H %.1f Solo %.1f Boia %d Fan %d Pump %d %s\n",
     temp,hum,soil,tankOk,fanOn,pumpOn,alert.c_str());
}

// Reutiliza HTTPS/TLS quando USE_LOCAL_HTTP=false; HTTP apenas em rede de teste.
bool request(const String& endpoint, const String& method, const String& payload, String& response){
  if(WiFi.status()!=WL_CONNECTED)return false;
  HTTPClient http;
  bool beginOk=false;
  WiFiClient plain;
  WiFiClientSecure secure;
  String url=String(SERVER_URL)+endpoint;
  if(USE_LOCAL_HTTP){beginOk=http.begin(plain,url);}
  else {secure.setCACert(ROOT_CA);beginOk=http.begin(secure,url);}
  if(!beginOk)return false;
  http.setTimeout(3500);
  http.addHeader("X-Device-Key",DEVICE_KEY);
  if(method=="POST")http.addHeader("Content-Type","application/json");
  int status=(method=="POST")?http.POST(payload):http.GET();
  if(status>=200&&status<300)response=http.getString();
  http.end();
  if(status<200||status>=300){Serial.printf("HTTP erro %d\n",status);return false;}
  return true;
}

void syncSettings(){
  String response;
  if(!request(String("/api/device/settings?device_id=")+DEVICE_ID,"GET","",response))return;
  JsonDocument doc;
  if(deserializeJson(doc,response))return;
  const char* receivedMode=doc["mode"] | "auto";
  mode=String(receivedMode)=="manual"?"manual":"auto";
  target=constrain(doc["temp_target"].as<float>(),10.0f,45.0f);
  soilMin=constrain(doc["soil_min"].as<float>(),0.0f,95.0f);
  soilStop=constrain(doc["soil_stop"].as<float>(),soilMin+1.0f,100.0f);
  manualFan=doc["manual_fan"].as<bool>();
  manualPump=doc["manual_pump"].as<bool>();
  controlLoop();
}

void sendTelemetry(){
  JsonDocument doc;
  doc["device_id"]=DEVICE_ID;
  if(sensorsOk){doc["temperature"]=temp;doc["humidity"]=hum;}
  else {doc["temperature"]=nullptr;doc["humidity"]=nullptr;}
  doc["soil"]=soil;
  doc["light"]=nullptr; // Sensor BH1750 opcional, nao instalado nesta versao.
  doc["tank_ok"]=tankOk;
  doc["fan"]=fanOn;doc["pump"]=pumpOn;
  doc["mode"]=mode;doc["alert"]=alert;
  String json,response;
  serializeJson(doc,json);
  request("/api/device/telemetry","POST",json,response);
}

void setup(){
  Serial.begin(115200);
  pinMode(FAN_PIN,OUTPUT);pinMode(PUMP_PIN,OUTPUT);
  digitalWrite(FAN_PIN,LOW);digitalWrite(PUMP_PIN,LOW);
  pinMode(FLOAT_PIN,INPUT_PULLUP);
  analogReadResolution(12);
  dht.begin();WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASS);
  lastSample=millis()-SAMPLE_MS;
  lastSync=millis()-SYNC_MS;
}
void loop(){
  unsigned long ms=millis();
  if(WiFi.status()!=WL_CONNECTED&&ms-lastWiFiRetry>WIFI_RETRY_MS){
    lastWiFiRetry=ms;WiFi.reconnect();
  }
  if(ms-lastSample>=SAMPLE_MS){lastSample=ms;sample();sendTelemetry();}
  if(ms-lastSync>=SYNC_MS){lastSync=ms;syncSettings();}
  // Proteger contra bomba travada mesmo se comunicacao bloquear ou sensores falharem.
  if(pumpOn && millis()-pumpStarted>=PUMP_MAX_MS)setPump(false);
}
