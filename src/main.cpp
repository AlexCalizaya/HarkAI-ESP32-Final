#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const int JSON_BUFFER = 256;//Tamaño del buffer, para este caso 256 bytes

// Definición de obj de librerías 
WiFiClient espClient; //Cliente del esp
PubSubClient client(espClient);
DynamicJsonDocument doc(JSON_BUFFER); // Define el tamaño del buffer 

struct Datos
{
  const char *campo1;
  const char *campo2;
};
Datos datatx;

struct Numeros
{
  const char *numero;
};
Numeros numbertx;


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

// void callback(char* topic, byte* payload, unsigned int length){
//   Serial.print("Received messages; ");
//   Serial.println(topic);
//   for(int i=0; i<length; i++){
//     Serial.print((char) payload[i]);
//   }
//   Serial.println();
// }

Datos read_json(byte *message)
{
  DynamicJsonDocument doc(JSON_BUFFER);
  DeserializationError error = deserializeJson(doc, (char *)message);

  if (error)
  {
    Serial.print(F("Error al analizar el JSON: "));
    Serial.println(error.c_str());
  }

  Datos datos;
  datos.campo1 = doc["nombre"];
  datos.campo2 = doc["codigo"];
  return datos;
}


void send_json(Numeros numbertx)
{
  // Crear un objeto JSON y asignar valores desde la estructura
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["numero"] = numbertx.numero;

  // Buffer para almacenar la cadena JSON
  char buffer[128];

  // Serializar el JSON directamente en el buffer
  size_t n = serializeJson(jsonDoc, buffer, sizeof(buffer));

  // Publicar el buffer directamente en el tema MQTT
  client.publish(outTopic, buffer, n);
}



void callback(char *topic, byte *message, int length)
{
    Serial.println("Mensaje recibido:");
    message[length] = '\0';
    Serial.println((char *)message);
    datatx=read_json(message);
    Serial.println(datatx.campo1);
    Serial.println(datatx.campo2);
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
    numbertx.numero = messages;
    send_json(numbertx);
    lastTime = millis();
  }
}

