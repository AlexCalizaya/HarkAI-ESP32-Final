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
  const char *sv_state; // Indica el estado del sistema (1: El sistema está conectado, 0 : El sistema no lo está)
  const char *count; // Indica el conteo de la infracción por ausencia de EPP después de los 15 segundos
  const char *detection; 
  const char *state;
  const char *mode;
  const char *id;
};
Datos datatx;

// Credenciales WiFi
const char *ssid = "HUAWEI-2.4G-By28";// Nombre del Wifi
const char *password = "DEC5uTCQ";// Contraseña

// Broker MQTT
const char *mqtt_server = "broker.emqx.io"; //Servidor mqtt del sistema
const char *brokerUser = "HarkAI-User";// Contraseña
const char *brokerPass = "nomeacuerdo";// Contraseña

// Tópicos
const char *inTopic = "CARRANZA/HARKAI"; // Del broker al ESP32
const char *outTopic = "CARRANZA/HARKAI/RX"; // Del ESP32 al broker

// ------------------------
//   Variables globales  //
// ------------------------

// Autenticadores

const char *id = "2"; // ID del muchacho

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
int num_count = 0; // Cuenta de reportes realizados
int state_change;
String mode_before = "1";

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

bool outputEnabled = false;  // Inicialmente deshabilitado

// Define the GPIO pins for the button and the output pin
const int buttonPin = 0;  // GPIO 0 for button input

// Variables to store the button stateButton and debouncing
bool flagEstadoBoton = HIGH;      // Variable to store the current button stateButton
bool estadoAnteriorBoton = HIGH;  // Variable to store the last button stateButton
unsigned long tiempoBoton = 0;
unsigned long tiempoDelayBoton = 50;
int estadoActualBoton=1;

bool stateButton = false;

// Funciones lógicas


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
  datos.sv_state = doc["server_state"];
  datos.count = doc["count"];
  datos.detection = doc["detection"];
  datos.state = doc["state"];
  datos.mode = doc["mode"];
  datos.id = doc["id"];
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

  // Buffer para almacenar la cadena JSON
  char buffer[128];

  // Serializar el JSON directamente en el buffer
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
  pinMode(buttonPin, INPUT_PULLUP);
  
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
  datatx.mode = "1";
  datatx.id=id; // Asignación de ID del dispositivo

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
    if (outputEnabled) {
        digitalWrite(pin, value);
    }
}

void states_changed(String estadoActual_modo){
  if(mode_before == estadoActual_modo){
    state_change = 0;
  }else{
    mode_before = estadoActual_modo;
    state_change = 1;
  }
}


void activacionManual(){

  estadoActualBoton = digitalRead(buttonPin); // Lee el estado del botón

  if (estadoActualBoton != estadoAnteriorBoton) {
    tiempoBoton = millis();
  }

  if ((millis() - tiempoBoton) > tiempoDelayBoton) {
    if (estadoActualBoton != flagEstadoBoton) {// Si se presiona LOW!=HIGH;
      flagEstadoBoton = estadoActualBoton;// Actualizamos el estado de flag en LOW
      
      if (estadoActualBoton == LOW) {
        stateButton = !stateButton; // Cambio de estado del boton
        if(String(datatx.detection)=="0") {
          Serial.println("Activar secuencia manual");
          datatx.sv_state = "1";
          datatx.count = "0";
          datatx.detection = "1";
          datatx.state = "0";
          //test_mode=buzzer;
          //.mode = "0";
          datatx.id=id; // Asignación de ID del dispositivo
        }else if(String(datatx.detection)=="1"){
          Serial.println("Desactivar secuencia manual");
          // flagLedAmbar = 0;
          // safeDigitalWrite(led_red, HIGH);
          // safeDigitalWrite(led_yellow, HIGH);
          // safeDigitalWrite(test_mode, HIGH);
          // tiempoDetected = millis();
          // tiempoAmbarDetected = millis();
          // frecuenciaParpadeo = frecuenciaParpadeo1;
          datatx.sv_state = "1";
          datatx.count = "0";
          datatx.detection = "0";
          datatx.state = "0";
          //test_mode=buzzer;
          //datatx.mode = "0";
          datatx.id=id; // Asignación de ID del dispositivo
        }
      }
    }
  }
  estadoAnteriorBoton = estadoActualBoton;
}


void cambio_modo(){
  
  if (String(datatx.id)==id) {
    if (String(datatx.mode) == "0") { // Modo normal
      Serial.println("Modo normal activado");
      Serial.println("LCD: MODO NORMAL ACTIVADO [0]");
      test_mode=hooter;
      outputEnabled = true;
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
    }
    else if (String(datatx.mode) == "1") // Modo prueba
    {
      Serial.println("Modo prueba activado");
      Serial.println("LCD: MODO PRUEBA ACTIVADO [1]");
      digitalWrite(hooter, HIGH);
      digitalWrite(buzzer, HIGH);
      test_mode=buzzer;
      outputEnabled = true;
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
    }
    else if (String(datatx.mode) == "2") // Modo silencio
    {
      Serial.println("Modo silencio activado");
      Serial.println("LCD: MODO SILENCIO ACTIVADO [2]");
      digitalWrite(led_red, HIGH);
      digitalWrite(led_green, HIGH);
      digitalWrite(led_yellow, HIGH);
      digitalWrite(hooter, HIGH);
      digitalWrite(buzzer, HIGH);
      test_mode=buzzer;
      outputEnabled = false;
      delay(1000);
      tiempoActual=millis();
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
    }
  }
}

//Funcion para parpadeo antes de conexion
void InicioSistema(int flagConexionSistema){
  tiempoActual=millis(); //Actualización del tiempo actual en millis
  digitalWrite(led_red, HIGH);
  digitalWrite(led_yellow, HIGH);

  if(flagConexionSistema==0){//Si es 0 el led parpadeará
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
  else if(flagConexionSistema==1){//Si es 1, el led se mantendrá encendido
    safeDigitalWrite(led_green, LOW);
    Serial.println("Led verde encendido");
  }
}

//Activa led rojo y manda confirmacion de cuenta a RN
void Finish()
{
  Serial.print("Se entro a la funcion Finish");
  Serial.print("LCD: FINALIZACION DE LA SECUENCIA - LED ROJO");
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
  Serial.println("Reporte emitido");
  if (String(datatx.count) == "1")
  {
    num_count = num_count + 1;
    Serial.print("Se han efectuado ");
    Serial.print(num_count);
    Serial.println("reportes");
  }

  datatx.sv_state = "1";
  datatx.count = "0";
  datatx.detection = "0";
  datatx.state = "0";

  frecuenciaParpadeo=frecuenciaParpadeo1;//Restablece el tiempo parpadeo del led amarillo al lento inicial
  flagparpadeoAmbar=0;//Restablece el flag de parpadeo ambar
  flagparpadeoVerde=0;//Restablece el flag de parpadeo verde
  tiempoDetected=tiempoActual;
  tiempoAmbarDetected=tiempoActual;
  Serial.print("Se terminó la funcion Finish");
  
}

//Activa parpadeo led ambar por 15seg
void Detected()
{
  if (String(datatx.detection) == "1" && String(datatx.state)=="0")
  {
    Serial.print("LCD: ESTADO DE DETECCIÓN - LED AMARILLO");
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

  else if (String(datatx.detection) == "0" && String(datatx.state)=="0")
  {                                // Si se corrigió la falta de epp
    flagLedAmbar = 0;
    safeDigitalWrite(led_yellow, HIGH); // Desactiva el relé ámbar <- Descomentar
    Serial.println("Led amarillo apagado ");
    tiempoDetected = millis();
    tiempoAmbarDetected = millis();
    frecuenciaParpadeo = frecuenciaParpadeo1;
  }

  // Verifica si han pasado 15 segundos desde la activación del relé
  if (flagLedAmbar == 1 && ((tiempoActual - tiempoDetected) >= tiempoFinalDetected) && String(datatx.state)=="0")
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
  Serial.println("LCD: ENTABLANDO CONEXIÓN WIFI");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Intento de conexión WiFi del ESP32
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    flagconectado=0;
    InicioSistema(flagconectado);
    Serial.print(".");
    delay(100);
    Serial.println("Memoria Libre: " + ESP.getFreeSketchSpace());
  }

  // Conexión exitosa a WiFi
  Serial.println("");
  Serial.println(WiFi.status()); // Se verifica la conexión de WiFi con status: WL_CONNECTED. Ref: https://www.arduino.cc/reference/en/libraries/wifi/wifi.status/
  Serial.println("WiFi connected");
  Serial.println("LCD: CONEXIÓN WIFI ESTABLECIDA CON ÉXITO");
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
    Serial.println("LCD: ENTABLANDO CONEXIÓN CON EL BROKER");
    Serial.println("Connecting to MQTT...");
    if (client.connect("HARKAI-ESP32-Prueba-2", brokerUser, brokerPass))
    {
      Serial.println("Conexión exitosa");
      Serial.println("LCD: CONEXIÓN CON EL BROKER ESTABLECIDA CON ÉXITO");
      Serial.print(client.state());
      client.subscribe(inTopic);
      datatx.sv_state="1"; //Indica que la conexión al broker es exitosa
      flagconectado=1;
    }
    else
    {
      Serial.println("LCD: CONEXIÓN FALLIDA CON EL BROKER - REINTENTANDO EN 3 SEGUNDOS");
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      flagconectado=0;
    }
    InicioSistema(flagconectado);
  }
}

void callback(char *topic, byte *message, int length)
{
    Serial.println("Mensaje recibido:");
    message[length] = '\0';
    Serial.println((char *)message);
    datatx=read_json(message);
    // Serial.println(String(datatx.mode));
    // Serial.println(mode_before);
    states_changed(String(datatx.mode));
    if(state_change){// Se verifica si se ha cambiado de modo
      cambio_modo(); 
    }
}

void setup() {
  Serial.println("LCD: INICIO DEL SETEO DE LA ESP32");
  Serial.begin(9600);
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
    client.loop();

    Serial.println("Memoria Libre: " + ESP.getFreeSketchSpace());

    tiempoActual=millis();

    activacionManual();
    
    
    

    Serial.println(String(datatx.mode));
    if(String(datatx.detection)=="0"){
      Serial.println("LCD: ESTADO PRINCIPAL - LED AMARRILLO APAGADO");
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
    }
    Serial.println("LCD: ESTADO DE DATATX: " + String(datatx.sv_state) + String(datatx.count) + String(datatx.detection) + String(datatx.state) + String(datatx.mode) + String(datatx.id));
    
    //Serial.println("Loop Principal");

    if ((tiempoActual - lastTime > frecuenciaPrincipal) && String(datatx.id)==id){
      //Serial.println("Dentro de la lógica principal");
      Detected();
      lastTime = millis();
    }
  }
}