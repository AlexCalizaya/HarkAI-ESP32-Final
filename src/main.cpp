#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

const int JSON_BUFFER = 256;//Tamaño del buffer, para este caso 256 bytes

// Definición de obj de librerías 
WiFiClient espClient; //Cliente del esp
PubSubClient client(espClient);
DynamicJsonDocument doc(JSON_BUFFER); // Define el tamaño del buffer 

struct Datos
{
  const char *sv_state; // Indica el estado del sistema (1: El sistema está conectado, 0 : El sistema no lo está)
  const char *count; // Indica el conteo de la infracción por ausencia de EPP después de los 15 segundos
  const char *detection; 
  const char *state;
  const char *mode;
  const char *id;
  const char *timestamp;
};
Datos datatx;

// Credenciales WiFi
const char *ssid = "HarkAI_Demo";// Nombre del Wifi
const char *password = "nomeacuerdo1956";// Contraseña


// Configuración de la IP estática (debe ser separada por comas)
IPAddress local_IP(192, 168, 0, 111);  // Cambia esto a la IP que deseas para el ESP32
IPAddress gateway(192, 168, 0, 1);     // Puerta de enlace, normalmente es la IP del router
IPAddress subnet(255, 255, 255, 0);     // Máscara de subred
IPAddress primaryDNS(8, 8, 8, 8);       // Servidor DNS primario
IPAddress secondaryDNS(8, 8, 4, 4);     // Servidor DNS secundario (opcional)

// Broker MQTT
const char *mqtt_server = "broker.emqx.io"; //Servidor mqtt del sistema

// Tópicos
const char *inTopic = "CARRANZA/HARKAI"; // Del broker al ESP32
const char *outTopic = "CARRANZA/HARKAI/RX"; // Del ESP32 al broker

// ------------------------
//   Variables globales  //
// ------------------------

// Autenticadores

const char *id = "1"; // ID del muchacho
String clientID = "HARKAI-ESP32-" + String(id);

// Tiempo

// Validados
long tiempoActual=0, lastTime=0, frecuenciaPrincipal=100;

// Por validar
unsigned long tiempoAmbarDetected, frecuenciaParpadeo, tiempoDetected, tiempoFinalDetected=15000;
unsigned long frecuenciaParpadeo1=1000, frecuenciaParpadeo2=500, frecuenciaParpadeo3=200;
unsigned long frencuenciaParpadeo;
unsigned long tiempoIniParpadeoSist=0;

int flagparpadeoAmbar=0;//Flag que indica el estado del parpadeo del led ámbar(0 apagado, 1 encendido)
int flagparpadeoVerde=0;//Flag que indica el estado del parpadeo del led verde(0 apagado, 1 encendido)
int flagconectado=0;//Flag que indica si el sistema está conectado o no
int flagLedAmbar = 0; // Indica cuando el estado del los 15 segundos está activo, es decir cuando se detecta falta de EPP
int state_changed_mode;
String mode_before = "1";
String timestamp = ""; // Variable global de timestamp

char messages[50];

//Pines de conexion;
// 4 Relay Module
const int led_red = 33; // Para rojo [4RM:IN1] 33
const int led_yellow = 25; // Para ámbar [4RM:IN2] 25
const int led_green = 26;  // Para verde [4RM:IN3] 26
const int buzzer = 27; // Para buzzer [4RM:IN4] 27
// 2 Relay Module
const int hooter = 32; // Para sirena [2RM:IN1] (cambiado para el debug) CABLE MORADO 32
const int rele2 = 35; // Para backup [2RM:IN2] CABLE NARANJA 35

// Variable global para testeo
int test_mode = hooter;

bool outputEnabled = true;  // Inicialmente deshabilitado
bool detectionActive = false;

String sv_state= "";
String count= "";
String detection= "";
String state= "";

// Funciones lógicas


Datos read_json(byte *message)
{
  DynamicJsonDocument doc(JSON_BUFFER);
  Serial.print("Entre");
  DeserializationError error = deserializeJson(doc, (char *)message);

  Serial.print("Debajo de DeserializationError");

  Serial.print("Error abajo si hay");
  Serial.println(error.c_str());

  if (error)
  {
    Serial.print(F("Error al analizar el JSON: "));
    Serial.println(error.c_str());
  }

  Datos datos;
  datos.sv_state = doc["server_state"];
  datos.count = doc["count"];          
  datos.detection = doc["detection"];  
  datos.state = doc["state"];          
  datos.mode = doc["mode"];            
  datos.id = doc["id"];                
  datos.timestamp = doc["timestamp"];  
  return datos;
}


void send_json(Datos datatx)
{
  // Crear un objeto JSON y asignar valores desde la estructura
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["server_state"] = datatx.sv_state;
  jsonDoc["count"] = datatx.count;
  jsonDoc["detection"] = datatx.detection;
  jsonDoc["state"] = datatx.state;
  jsonDoc["mode"] = datatx.mode;
  jsonDoc["id"] = String(id);
  jsonDoc["timestamp"] = timestamp;

  // Buffer para almacenar la cadena JSON
  char buffer[128];

  // Serializar el JSON directamente en el buffer+
  size_t n = serializeJson(jsonDoc, buffer, sizeof(buffer));

  // Publicar el buffer directamente en el tema MQTT
  client.publish(outTopic, buffer, n);
}

//Inicializa pines ESP32
void Init_leds()
{
  pinMode(led_green, OUTPUT);
  pinMode(led_red, OUTPUT);
  pinMode(led_yellow, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(hooter, OUTPUT);
  pinMode(rele2, OUTPUT);
  
  digitalWrite(led_green, HIGH);
  digitalWrite(led_red, HIGH);
  digitalWrite(led_yellow, HIGH);
  digitalWrite(buzzer, HIGH);
  digitalWrite(hooter, HIGH); 
  digitalWrite(rele2, HIGH); 
}

//Determina las condiciones iniciales de la energización del muchacho
void Init_System()
{
  //////////////////////////////////////
  // Inicialización del sistema
  //////////////////////////////////////
  datatx.sv_state = "1";
  datatx.count = "0";
  datatx.detection = "0";
  datatx.state = "0";
  datatx.mode = "0";
  datatx.id=id; // Asignación de ID del dispositivo
  datatx.timestamp="";

  sv_state = "1";
  count = "0";
  detection = "0";
  state = "0";
  id=id; // Asignación de ID del dispositivo
  timestamp="";

  digitalWrite(led_green, LOW);
  digitalWrite(led_red, LOW);
  digitalWrite(led_yellow, LOW);

  tiempoActual=millis();
  tiempoAmbarDetected=millis();
  tiempoDetected=millis();

  frecuenciaParpadeo=frecuenciaParpadeo1;
  //////////////////////////////////////
}

void safeDigitalWrite(int pin, int value) {
    if (outputEnabled || pin == led_green) {
        digitalWrite(pin, value);
    }
}

//Funcion para parpadeo antes de conexion
void InicioSistema(int flagConexionSistema){
  tiempoActual=millis(); //Actualización del tiempo actual en millis

  if(flagConexionSistema==0){//Si es 0 el led parpadeará
    if(((tiempoActual - tiempoIniParpadeoSist) >= frecuenciaParpadeo1)){
      if(flagparpadeoVerde==0){
        safeDigitalWrite(led_green, LOW); 
        safeDigitalWrite(led_yellow, LOW);
        safeDigitalWrite(led_red, LOW); 
        Serial.println("Todos los leds encendidos");
      }
      if(flagparpadeoVerde==1){
        safeDigitalWrite(led_green, HIGH); 
        safeDigitalWrite(led_yellow, HIGH); 
        safeDigitalWrite(led_red, HIGH);
        Serial.println("Todos los leds apagados");
      }
      flagparpadeoVerde= 1 - flagparpadeoVerde;
      tiempoIniParpadeoSist = millis(); // Guarda el tiempo de inicio del millis
    }
  }
  else if(flagConexionSistema==1){//Si es 1, el led se mantendrá encendido
    digitalWrite(led_red, HIGH);
    digitalWrite(led_yellow, HIGH);
    if(((tiempoActual - tiempoIniParpadeoSist) >= frecuenciaParpadeo1)){
      if(flagparpadeoVerde==0){
        safeDigitalWrite(led_green, LOW); // Activa el relé verde
        Serial.println("Led verde encendido");
      }
      if(flagparpadeoVerde==1){
        safeDigitalWrite(led_green, HIGH); // Desactiva el relé verde
        Serial.println("Led verde apagado");
      }
      flagparpadeoVerde= 1 - flagparpadeoVerde;
      tiempoIniParpadeoSist = millis(); // Guarda el tiempo de inicio del millis
    }
  }
  else if(flagConexionSistema==2){//Si es 2, el led se mantendrá encendido
    digitalWrite(led_red, HIGH);
    digitalWrite(led_yellow, HIGH);
    safeDigitalWrite(led_green, LOW);
    Serial.println("Led verde encendido");
  }
}

void cambio_modo(){
  
  if (String(datatx.id)==id) {
    if (String(datatx.mode) == "0") { // Modo normal
      Serial.println("Modo normal activado");
      test_mode=hooter;
      outputEnabled = true;
      flagconectado=2;
      InicioSistema(flagconectado);
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
      frecuenciaParpadeo=frecuenciaParpadeo1;
    }
    else if (String(datatx.mode) == "1") // Modo prueba
    {
      Serial.println("Modo prueba activado");
      digitalWrite(hooter, HIGH);
      digitalWrite(buzzer, HIGH);
      test_mode=buzzer;
      outputEnabled = true;
      flagconectado=2;
      InicioSistema(flagconectado);
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
      frecuenciaParpadeo=frecuenciaParpadeo1;
    }
    else if (String(datatx.mode) == "2") // Modo silencio
    {
      Serial.println("Modo silencio activado");
      digitalWrite(led_red, HIGH);
      digitalWrite(led_green, LOW);
      digitalWrite(led_yellow, HIGH);
      digitalWrite(hooter, HIGH);
      digitalWrite(buzzer, HIGH);
      test_mode=buzzer;
      outputEnabled = false;
      flagconectado=2;
      InicioSistema(flagconectado);
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
      frecuenciaParpadeo=frecuenciaParpadeo1;
    }
  }
}

void states_changed_mode(String estadoActual_modo){
  if(mode_before == estadoActual_modo){
    state_changed_mode = 0;
  }else{
    mode_before = estadoActual_modo;
    state_changed_mode = 1;
  }
}

void states_changed_general(String estadoActual_id){
  if(String(id) == estadoActual_id){
    if(String(datatx.sv_state)=="1" || String(datatx.sv_state)=="0"){sv_state=datatx.sv_state;}
    if(String(datatx.count)=="1" || String(datatx.count)=="0"){count=datatx.count;}
    if(String(datatx.detection)=="1" || String(datatx.detection)=="0"){detection=datatx.detection;}
    if(String(datatx.state)=="1" || String(datatx.state)=="0"){state=datatx.state;}
    timestamp=datatx.timestamp;
    states_changed_mode(String(datatx.mode));
    if(state_changed_mode){// Se verifica si se ha cambiado de modo
      cambio_modo(); 
    }
  }
}

//Activa led rojo y manda confirmacion de cuenta a RN
void Finish()
{
  Serial.print("Se entro a la funcion Finish");
  Serial.println("Led rojo encendido");
  safeDigitalWrite(led_red, LOW); //<- Descomentar
  safeDigitalWrite(test_mode, LOW);
  delay(5000);
  Serial.println("Led rojo apagado");
  safeDigitalWrite(led_red, HIGH); //<- Descomentar
  safeDigitalWrite(test_mode, HIGH);
  datatx.sv_state = "1"; // server_state: 
  datatx.count = "1"; // count: Cuenta de 15 seg completa
  datatx.detection = "0"; // detection: 
  datatx.state = "1"; // state: ESP32 envia a la RN
  send_json(datatx);

  datatx.sv_state = "1";
  datatx.count = "0";
  datatx.detection = "0";
  datatx.state = "0";

  sv_state = "1";
  count = "0";
  detection = "0";
  state = "0";


  frecuenciaParpadeo=frecuenciaParpadeo1;//Restablece el tiempo parpadeo del led amarillo al lento inicial
  flagparpadeoAmbar=0;//Restablece el flag de parpadeo ambar
  tiempoDetected=tiempoActual;
  tiempoAmbarDetected=tiempoActual;
  flagconectado=2;
  InicioSistema(flagconectado);
  Serial.print("Se terminó la funcion Finish");
}

//Activa parpadeo led ambar por 15seg
void Detected()
{
  if (detection == "1" && state=="0")
  {
    Serial.println("Detected");
    if((tiempoActual - tiempoAmbarDetected) >= frecuenciaParpadeo){
      if(flagparpadeoAmbar==0){
        safeDigitalWrite(led_yellow, LOW); // Activa el rele ambar <- Descomentar
        Serial.println("Led amarillo encendido");
      }
      if(flagparpadeoAmbar==1){
        safeDigitalWrite(led_yellow, HIGH); // Desactiva el rele ambar <- Descomentar
        Serial.println("Led amarillo apagado");
      }
      flagparpadeoAmbar= 1 - flagparpadeoAmbar;
      tiempoAmbarDetected = millis(); // Guarda el tiempo de inicio del millis
    }
    if((tiempoActual - tiempoDetected) >= 5000){
      frecuenciaParpadeo=frecuenciaParpadeo2;
    }
    if((tiempoActual - tiempoDetected) >= 10000){
      frecuenciaParpadeo=frecuenciaParpadeo3;
    }
    flagLedAmbar = 1;
  }

  else if (detection == "0" && state=="0")
  {                                // Si se corrigió la falta de epp
    flagLedAmbar = 0;
    safeDigitalWrite(led_yellow, HIGH); // Desactiva el relé ámbar <- Descomentar
    Serial.println("Led amarillo apagado ");
    tiempoDetected = millis();
    tiempoAmbarDetected = millis();
    frecuenciaParpadeo = frecuenciaParpadeo1;
  }

  // Verifica si han pasado 15 segundos desde la activación del relé
  if (flagLedAmbar == 1 && ((tiempoActual - tiempoDetected) >= tiempoFinalDetected) && state=="0")
  {
    safeDigitalWrite(led_yellow, HIGH); // Desactiva el relé automáticamente <- Descomentar
    Serial.println("Led amarillo apagado"); 
    flagLedAmbar = 0;
    Finish();
  }
}


// Funciones de configuración

// Función Inicio WiFi
void wifiInit() {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Intento de conexión WiFi del ESP32
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    flagconectado=0;
    InicioSistema(flagconectado);
    Serial.print(".");
    //delay(10);
  }

  // Conexión exitosa a WiFi
  Serial.println("");
  Serial.println(WiFi.status()); // Se verifica la conexión de WiFi con status: WL_CONNECTED. Ref: https://www.arduino.cc/reference/en/libraries/wifi/wifi.status/
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // Se printea la IP de conexión
  flagconectado=1;
  InicioSistema(flagconectado);
}

//Funcion para reconectar con broker
void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect(clientID.c_str()))
    {
      Serial.println("Conexión exitosa al muchacho con ID: " + clientID);
      Serial.print(client.state());
      client.subscribe(inTopic);
      datatx.sv_state="1"; //Indica que la conexión al broker es exitosa
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
    }
    flagconectado=1;
    InicioSistema(flagconectado);
  }
  flagconectado=2;
  InicioSistema(flagconectado);
}

void callback(char *topic, byte *message, int length)
{
    Serial.println("Mensaje recibido:");
    message[length] = '\0';
    Serial.println((char *)message);
    datatx=read_json(message);

    states_changed_general(String(datatx.id));
}

void setup() {
  Serial.begin(9600);

  // Configuración de IP estática antes de conectar al WiFi
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Fallo en la configuración de IP estática");
  }
  ArduinoOTA.begin();  // Starts OTA
  Init_leds();
  Init_System();
  wifiInit();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if(WiFi.status() != WL_CONNECTED){ // verifica si aún existe conexión WiFi
    wifiInit();
  }else{
    if (!client.connected()){
      reconnect();
    }
    ArduinoOTA.handle();
    client.loop();

    tiempoActual=millis();
    Serial.println("Fuera del loop json");

    if (sv_state=="1" && state=="0"){
      Serial.println(sv_state);
      Serial.println(count);
      Serial.println(detection);
      Serial.println(state);
      //Serial.println(mode);
      Serial.println(id);
      Serial.println(timestamp);
      //delay(500);

      if(detection=="0"){
        safeDigitalWrite(led_yellow, HIGH); // Activa el rele ambar <- Descomentar
        tiempoAmbarDetected=millis();
        tiempoDetected=millis();
        frecuenciaParpadeo=frecuenciaParpadeo1;
      }
      
      if ((tiempoActual - lastTime > frecuenciaPrincipal)){
        Serial.println("Dentro de la lógica principal");
        Detected();
        lastTime = millis();
      }
    }
  }
}