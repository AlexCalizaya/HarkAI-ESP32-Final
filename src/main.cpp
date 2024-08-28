#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
//#include <ArduinoJson.h>

// Definición de obj de librerías 
WiFiClient espClient; //Cliente del esp
PubSubClient client(espClient);

// Credenciales WiFi
const char *ssid = "HUAWEI-2.4G-By28";// Nombre del Wifi
const char *password = "DEC5uTCQ";// Contraseña

// Broker MQTT
const char *mqtt_server = "broker.emqx.io"; //Servidor mqtt del sistema
const char *brokerUser = "HarkAI-User";// Contraseña
const char *brokerPass = "nomeacuerdo";// Contraseña

// Tópicos
const char *inTopic = "/CALI/IN";
const char *outTopic = "/CALI/OUT";
// ------------------------
// Variables globales     -
// ------------------------

// Tiempo
long currentTime, lastTime;
int count = 0;
char messages[50];

// Función Inicio WiFi
void wifiInit() {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Intento de conexión WiFi del ESP32
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  // Conexión exitosa a WiFi
  Serial.println("");
  Serial.println(WiFi.status()); // Se verifica la conexión de WiFi con status: WL_CONNECTED. Ref: https://www.arduino.cc/reference/en/libraries/wifi/wifi.status/
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // Se printea la IP de conexión
}

//Funcion para reconectar con broker
void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect("HARKAI-ESP32-Prueba", brokerUser, brokerPass))
    {
      Serial.println("Conexión exitosa");
      Serial.print(client.state());
      client.subscribe(inTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
    }
  }
}

// Funcion Callback

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Received messages; ");
  Serial.println(topic);
  for(int i=0; i<length; i++){
    Serial.print((char) payload[i]);
  }
  Serial.println();
}



void setup() {
  Serial.begin(9600);
  wifiInit();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()){
    reconnect();
  }
  client.loop();

  currentTime=millis();
  if (currentTime - lastTime > 2000){
    count++;
    snprintf(messages, 75, " Count: %ld", count);
    Serial.print("Sending messages: ");
    Serial.println(messages);
    client.publish(outTopic, messages);
    lastTime = millis();
  }
}

