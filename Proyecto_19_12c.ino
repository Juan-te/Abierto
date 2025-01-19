


#include <string>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include "Button2.h"
#include <WiFiManager.h>

WiFiManager wifi;

#define BUTTON_PIN 9
#define rgbLedWrite neopixelWrite
#define __HTTPS__
#define SENSOR 30000

TaskHandle_t Manejador_tarea_sensor;
TaskHandle_t Manejador_tarea_actualiza;
TimerHandle_t temp_sensor;
QueueHandle_t cola_sensor;

#ifdef __HTTPS__
#define OTA_URL "https://iot.ac.uma.es:1880/esp32-ota/update"  // Address of OTA update server
#else
#define OTA_URL "http://172.16.53.122:1880/esp32-ota/update"  // Address of OTA update server
#endif
DHTesp dht;

// Nombre del firmware (válido para linux/macos '/' y Windows '\\')
#define FW_BOARD_NAME ""
#define HTTP_OTA_VERSION strrchr(strrchr(__FILE__, '/') ? __FILE__ : "", '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') + 1

// definimos macro para indicar función y línea de código en los mensajes
#define DEBUG_STRING "[" + String(__FUNCTION__) + "():" + String(__LINE__) + "] "

WiFiClient wClient;
PubSubClient mqtt_client(wClient);

Button2 button;

// Update these with values suitable for your network.
const String ssid = "infind";
const String password = "1518wifi";
const String mqtt_server = "iot.ac.uma.es";
const String mqtt_user = "II11";
const String mqtt_pass = "mM3PZkAN";

// cadenas para topics e ID
String ID_PLACA;
String topic_PUB_conexion;
String topic_PUB_datos;
String topic_SUB_config;
String topic_SUB_brillo;
String topic_PUB_brillo_st;
String topic_SUB_led;
String topic_PUB_ledcol;
String topic_SUB_swcmd;
String topic_PUB_swst;
String topic_SUB_Fota;

String ChipID;

int envia=30000;
int actualiza=0;
int velocidad=5;
int SWITCH=1;

int R=50,G=50,B=50,Rojo,Verde,Azul,Rw,Gw,Bw;
const char* id = "12412";
bool led=false;

unsigned long ultimo_mensaje_fota=0;

//-----------------------------------------------------
// funciones para progreso de OTA
//-----------------------------------------------------
void inicio_OTA() {
  Serial.println(DEBUG_STRING + "Nuevo Firmware encontrado. Actualizando...");
}
void final_OTA() {
  Serial.println(DEBUG_STRING + "Fin OTA. Reiniciando...");
}
void error_OTA(int e) {
  Serial.println(DEBUG_STRING + "ERROR OTA: " + String(e) + " " + httpUpdate.getLastErrorString());
}
void progreso_OTA(int x, int todo) {
  int progreso = (int)((x * 100) / todo);
  String espacio = (progreso < 10) ? "  " : (progreso < 100) ? " "
                                                             : "";
  if (progreso % 10 == 0) Serial.println(DEBUG_STRING + "Progreso: " + espacio + String(progreso) + "% - " + String(x / 1024) + "K de " + String(todo / 1024) + "K");
}
//-----------------------------------------------------
void intenta_OTA() {
  Serial.println("---------------------------------------------");
  Serial.print(DEBUG_STRING + "MAC de la placa: ");
  Serial.println(WiFi.macAddress());
  Serial.println(DEBUG_STRING + "Comprobando actualización:");
  Serial.println(DEBUG_STRING + "URL: " + OTA_URL);
  Serial.println("---------------------------------------------");
  httpUpdate.setLedPin(RGB_BUILTIN);
  httpUpdate.onStart(inicio_OTA);
  httpUpdate.onError(error_OTA);
  httpUpdate.onProgress(progreso_OTA);
  httpUpdate.onEnd(final_OTA);

#ifdef __HTTPS__
#include <NetworkClientSecure.h>
  NetworkClientSecure wClient;
  // La lectura sobre SSL puede ser lenta, poner timeout suficiente
  wClient.setTimeout(12);  // timeout en segundos
  wClient.setInsecure();   // no comprobar el certificado del servidor
#else
  WiFiClient wClient;
#endif

  switch (httpUpdate.update(wClient, OTA_URL, HTTP_OTA_VERSION)) {
    case HTTP_UPDATE_FAILED:
      Serial.println(DEBUG_STRING + "HTTP update failed: Error (" + String(httpUpdate.getLastError()) + "): " + httpUpdate.getLastErrorString());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(DEBUG_STRING + "El dispositivo ya está actualizado");
      break;
    case HTTP_UPDATE_OK:
      Serial.println(DEBUG_STRING + "OK");
      break;
  }
}

//-----------------------------------------------------
void conecta_wifi() {
  Serial.println("Connecting to " + ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected, IP address: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  String ultima = "{\"online\":false,\"ChipId\": " + String(ChipID) + "}";
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(), topic_PUB_conexion.c_str(), 2, true, ultima.c_str())) {
      Serial.println(" conectado a broker: " + mqtt_server);
      mqtt_client.subscribe(topic_SUB_config.c_str());
      mqtt_client.subscribe(topic_SUB_brillo.c_str());
      mqtt_client.subscribe(topic_SUB_led.c_str());
      mqtt_client.subscribe(topic_SUB_swcmd.c_str());
      mqtt_client.subscribe(topic_SUB_Fota.c_str());
    } else {
      Serial.println("ERROR:" + String(mqtt_client.state()) + " reintento en 5s");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  String nuevo = "{\"online\":true,\"ChipId\": " + String(ChipID) + "}";
  mqtt_client.publish(topic_PUB_conexion.c_str(), nuevo.c_str(), true);
}
  /////////////////////////////////////////////////////////
  // BOTON
void singleClick(Button2& btn) {
    switch (led){
    case true:
      digitalWrite(RGB_BUILTIN,LOW);
    break;
    case false:
      digitalWrite(RGB_BUILTIN,HIGH);
    break;
    }
  led = not (led);
  //Serial.println("click\n");
}
void longClick(Button2& btn) {
    intenta_OTA();
    //Serial.println("click\n");
}
void doubleClick(Button2& btn) {
    rgbLedWrite(RGB_BUILTIN,R,G,B);
    led = true;
    //Serial.println("click\n");
}
/*void tripleClick(Button2& btn) {
    Serial.println("click\n");
}
void longClickDetected(Button2& btn) {
    Serial.println("long click detected");
}*/
//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  String mensaje = String(std::string((char*)payload, length).c_str());
  Serial.println("Mensaje recibido [" + String(topic) + "] " + mensaje);
 
 
  if (String(topic) == topic_SUB_swcmd) {
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, mensaje);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    int level = doc["level"];  // 1
    String id2="12345";
    //const char* id = doc["id"];
    if (level == 1 and SWITCH==1) {
      digitalWrite(5, HIGH);
    } 
    else if(level == 0 and SWITCH==0){
    digitalWrite(5, HIGH);}
    else {
      digitalWrite(5, LOW);
    }

    String swst = "{\"level\":" + String(level)  + ",\"id\":\"" + String(id2) +"\",\"ChipId\": \"" + String(ChipID) + "\"}";
    mqtt_client.publish(topic_PUB_swst.c_str(), swst.c_str(), true);
  }
  if (String(topic) == topic_SUB_brillo) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, mensaje);
      if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    int valor = doc["level"];
    Rw=Rojo;Gw=Verde;Bw=Azul;
    Rojo=(R * valor) / 100;
    Verde=(G * valor) / 100;
    Azul=(B * valor) / 100;
    led=true;
    cambia_color();
    //rgbLedWrite(RGB_BUILTIN,Rojo,Verde ,Azul);
    String brst="{\"LED\":"+String(valor)+",\"id\":\" " + String(id) + "\",\"ChipId\":\"" + String(ChipID) + "\", \"origen\":\"mqtt\"}";
    mqtt_client.publish(topic_PUB_brillo_st.c_str(), brst.c_str());
  
  }

  if(String(topic)==topic_SUB_led){
    
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, mensaje);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    R = doc["r"];
    G = doc["g"];
    B = doc["b"];
    Rw=Rojo;Gw=Verde;Bw=Azul;
    Rojo=R;   Verde=G;    Azul=B;
    cambia_color();
    //rgbLedWrite(RGB_BUILTIN,Rojo,Verde ,Azul );
    led=true;
    String col="{\"R\":" + String(R) + "%,\"G\":" + String(G) + "%,\"B\":" + String(B) + "%,\"id\": " + String(id) + ",\"ChipId\": " + String(ChipID) + "}";
    mqtt_client.publish(topic_PUB_ledcol.c_str(), mensaje.c_str());
  }
  if(String(topic)==topic_SUB_config)
  {
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, mensaje);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    return;
    String envia_ = doc["envia"];
  String actualiza_ = doc["actualiza"];
  String velocidad_ = doc["velocidad"];
  String SWITCH_ = doc["SWITCH"];
  if(envia_!="null")
  {
    envia = doc["envia"]*1000;
  }
  if(actualiza_!="null")
  {
    actualiza = doc["actualiza"];
  }
  if(velocidad_!="null")
  {
    velocidad = doc["velocidad"];
  }
  if(SWITCH_!="null")
  {
    SWITCH = doc["SWITCH"];
  }
  }
  if(String(topic)==topic_SUB_Fota)
  {
    if(mensaje=="{\"actualiza\":true}")
    {
      intenta_OTA();
    }
  }
}
  }

  

void Tarea_actualiza( void * parameter )
{
  while(true){
  if (!mqtt_client.connected())
  {
    conecta_mqtt();
  }
  mqtt_client.loop();
  button.loop();
  unsigned long ahora = millis();
  if ((ahora - ultimo_mensaje_fota >= (actualiza*1000)) && actualiza!=0)
  {
    ultimo_mensaje_fota=ahora;
    intenta_OTA();
  }
  vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Tarea_sensor( void * parameter )
{
  temp_sensor= xTimerCreate("Temporizador", envia/portTICK_RATE_MS, pdTRUE,  ( void * ) 0, callback_sensor);
  xTimerStart( temp_sensor, 0 );
  bool datos=false;
  while(true)
  {
   xQueueReceive( cola_sensor, &( datos ), portMAX_DELAY);
   if(datos)
   {
    float hum = dht.getHumidity();
    float temp = dht.getTemperature();
    JsonDocument doc;
    String output;

    doc["CHIPID"] = String(ChipID);
    doc["Uptime"] = millis();

    JsonObject DHT11 = doc["DHT11"].to<JsonObject>();
    DHT11["temp"] = temp;
    DHT11["hum"] = hum;

    JsonObject Wifi = doc["Wifi"].to<JsonObject>();
    Wifi["SSId"] = "infind";
    Wifi["IP"] = WiFi.localIP().toString();
    Wifi["RSSI"] = WiFi.RSSI();

    doc.shrinkToFit();  // optional

    serializeJson(doc, output);
    mqtt_client.publish(topic_PUB_datos.c_str(), output.c_str());
    datos=false;
   }
  }
}
void callback_sensor( TimerHandle_t xTimer )
{
  bool datos=true;
  xQueueSend( cola_sensor,( void * ) &datos,( TickType_t ) 0 );
}

void cambia_color(){
float dr=(Rojo-Rw)/100;float dg=(Verde-Gw)/100;float db=(Azul-Bw)/100;
for (int i=0;i<100;i++)
{
  rgbLedWrite(RGB_BUILTIN,floor(Rojo+dr),floor(Verde+dg) ,floor(Azul+db));
  Rojo=Rojo+dr;   Verde=Verde+dg;    Azul=Azul+db;
  delay(velocidad*10);// velocidad en segundos por 1000 para ms y entre 100 pasos que da el for
}
}


//-----------------------------------------------------
//     SETUP
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  //Serial.println();
  Serial.println("Empieza setup...");
  // crea topics usando id de la placa
 uint32_t chip = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chip |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String ID_PLACA="ESP_" + String( chip );
  pinMode(5, OUTPUT);
  //conecta_wifi();
  wifi.autoConnect("MiDispositivo");
  ChipID=String(WiFi.getHostname());
  Serial.println("Identificador de placa :" + ChipID);
 

  topic_PUB_conexion = "II11/" + String(ChipID) + "/conexion";
  topic_PUB_datos = "II11/" + String(ChipID) + "/datos";
  topic_SUB_config = "II11/" + String(ChipID) + "/config";
  topic_SUB_brillo = "II11/" + String(ChipID) + "/led/brillo";
  topic_PUB_brillo_st = "II11/" + String(ChipID) + "/led/brillo/estado";
  topic_SUB_led = "II11/" + String(ChipID) + "/led/color";
  topic_PUB_ledcol = "II11/" + String(ChipID) + "/led/color/estado";
  topic_SUB_swcmd = "II11/" + String(ChipID) + "/switch/cmd";
  topic_PUB_swst = "II11/" + String(ChipID) + "/switch/status";
  topic_SUB_Fota = "II11/" + String(ChipID) + "/FOTA";

    Serial.println(topic_PUB_swst);

  mqtt_client.setServer(mqtt_server.c_str(), 1883);
  mqtt_client.setBufferSize(512);  // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  intenta_OTA();
  dht.setup(4, DHTesp::DHT11);
  digitalWrite(RGB_BUILTIN,LOW);
    /////////////////////////////////////////////////////////
  // BOTON
  button.begin(BUTTON_PIN);
  button.setClickHandler(singleClick);
  button.setLongClickHandler(longClick);
  button.setDoubleClickHandler(doubleClick);
  //button.setTripleClickHandler(tripleClick);
  /////////////////////////////////////////////////////////
  cola_sensor = xQueueCreate( 10, sizeof( bool) );
  xTaskCreate(Tarea_sensor,"Tarea sensor", 2048, (void*)0, 10, &Manejador_tarea_sensor);
  xTaskCreate(Tarea_actualiza,"Tarea actualizador", 2048, (void*)0, 2, &Manejador_tarea_actualiza);
  vTaskDelete(NULL);
}

void loop() {
  
}
