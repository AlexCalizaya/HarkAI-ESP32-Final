# HarkAI ESP32

El siguiente documento detalla la programación del código diseñado para el funcionamiento del sistema integrado de detección de EPPs (muchacho).

# Instalación de código

Para la instalación del código es necesario clonar el proyecto subido en el presente repositorio en la dirección del la carpeta en la que se desea administrar el proyecto.

```bash
gh repo clone AlexCalizaya/HarkAI-ESP32-Final
```
Posterior a ello, ejecutar Visual Studio Code, importar el proyecto en PlatformIO y seleccionar la tarjeta "Espressif ESP32 Dev Module".

A continuación, se abrirá el proyecto en el entorno de desarrollo en la cuál podrá hacer las modificaciones o integraciones concernientes.

# Explicación del código (last update)

El código consiste en el control de dispositivos visuales y auditivos para la alerta ante incidentes detectados por el Centro de procesamiento de Inteligencia Artificial diseñado por HarkTech.

## Librerías
Se importan las siguientes librerías:

```Arduino
#include <Arduino.h> // Framework utilizado
#include <WiFi.h> // Conexión WiFi
#include <PubSubClient.h> // Conexión con el broker
#include <ArduinoJson.h> // Manejo de objetos json
```
## Variables globales
Las variables utilizadas son las siguientes:

Estructura de objetos principales:
```Arduino
const int JSON_BUFFER = 256; //Tamaño del buffer
// Indica el conteo de la infracción por ausencia de EPP después de los 15 segundos
WiFiClient espClient; //Cliente Wifi
PubSubClient client(espClient); // Cliente json
DynamicJsonDocument doc(JSON_BUFFER); // Crea un json dinámico

struct Datos
{
  const char *sv_state; // Indica el estado del sistema (1: El sistema está conectado, 0 : El sistema no lo está)
  const char *count; // Indica la infracción por ausencia de EPP después de los 15 segundos
  const char *detection; // Señal que dirije el inicio o detención del conteo de los 15 segundos del SMP
  const char *state; // Indica quien realiza la comunicación [0: IA, 1: SMP]
  const char *mode; // Indica el modo en el que se encuentra el SMP [0: Modo Industrial (Normal), 1: Modo Desarrollo (Prueba), 2: Modo silencio]
  const char *id; // Identificador de cada SMP
};

Datos datatx; // Se define el objeto Datos
```

Variables de comunicación:

```Arduino
// Credenciales WiFi
const char *ssid = "S21 FE de Alex";// Nombre del Wifi
const char *password = "12345678";// Contraseña

// Broker MQTT
const char *mqtt_server = "broker.emqx.io"; //Servidor mqtt del sistema

// Tópicos
const char *inTopic = "CARRANZA/HARKAI"; // Del broker al ESP32
const char *outTopic = "CARRANZA/HARKAI/RX"; // Del ESP32 al broker

const char *id = "4"; // ID del muchacho
String clientID = "HARKAI-ESP32-" + String(id); // Arma un string para crear el ClientID - (Identificador de dispositivo IoT para el broker)

char messages[50]; // Se define el tamaño de la cadena de lectura del broker (arbitrario)
```

Variables lógicas:
```Arduino
int flagparpadeoAmbar=0; // Flag que indica el estado del parpadeo del led ámbar (0 apagado, 1 encendido)
int flagparpadeoVerde=0; // Flag que indica el estado del parpadeo del led verde (0 apagado, 1 encendido)
int flagconectado=0; // Flag que indica el inicio del sistema [0: Energizado - Conectando a internet, 1: Conectando al broker - SMP conectado a la red sin internet, 2: Sistema en funcionamiento óptimo]
int flagLedAmbar = 0; // Indica cuando el estado del los 15 segundos está activo, es decir cuando se detecta falta de EPP
int state_change // Indica el cambio de estado de las variables;
String mode_before = "0"; // Inicializo el modo en Modo Industrial

//Pines de conexion;
// 4 Relay Module
const int led_red = 33; // Para rojo [4RM:IN1] 33
const int led_yellow = 25; // Para ámbar [4RM:IN2] 25
const int led_green = 26;  // Para verde [4RM:IN3] 26
const int buzzer = 27; // Para buzzer [4RM:IN4] 27
// 2 Relay Module
const int hooter = 32; // Para bocina [2RM:IN1] (cambiado para el debug) CABLE MORADO 32
const int rele2 = 35; // Para backup [2RM:IN2] CABLE NARANJA 35

// Variable global para testeo
int test_mode = hooter;

bool outputEnabled = true;  // Bandera booleana para definir cuando permitir el funcionamiento de las luces y bocinas (digitalWrite). Esto puede ser definido según el modo definido
```

Variables temporales:

```Arduino
// Tiempo

// Validados
long tiempoActual=0; // Variable general que maneja el tiempo del SMP
long lastTime=0; // Bandera temporal de la lógica principal
long frecuenciaPrincipal=100; // Frecuencia de lectura de la lógica principal. Se coloca para evitar llenar el buffer de sistema embebido.

unsigned long tiempoAmbarDetected; // Bandera temporal del parpadeo de Detected activado
unsigned long frecuenciaParpadeo; // Variable dinámica para cambiar la frecuencia de parpadeo
unsigned long tiempoDetected; // Bandera para cambiar la frecuencia del led ámbar en Detected
unsigned long tiempoFinalDetected=15000; // Variable para definir el tiempo final de 
unsigned long frecuenciaParpadeo1=1000; // Constante de frecuencia temporal 1
unsigned long frecuenciaParpadeo2=500; // Constante de frecuencia temporal 2
unsigned long frecuenciaParpadeo3=200; // Constante de frecuencia temporal 3
unsigned long frencuenciaParpadeo; // Variable dinámica que cambia según la frecuencia de tiempo asignada (1,2,3)
unsigned long tiempoIniParpadeoSist=0; // Bandera para controlar el tiempo de parpadeo del inicio de sistema
```

## Funciones
Las funciones utilizadas se dividen en funciones de comunicación y lógica:

### Función de comunicación
Las siguientes son las funciones utilizadas para la comunicación del ESP32 con el Broker:

### Read_json
Funcion para la lectura del json recibido por el broker TopicIn.
```Arduino
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
```
### Send_json
Función para enviar json al broker TopicOut.

```Arduino
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

```

### Función de lógica
Las siguientes son funciones utilizadas para la secuencia del sistema:

### Init_leds
Función para el inicializar los leds y dispositivos conectados al SMP.

```Arduino
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
```

### Init_System
Determina las condiciones iniciales del SMP al energizar el sistema.

```Arduino
void Init_System()
{
  datatx.sv_state = "1";
  datatx.count = "0";
  datatx.detection = "0";
  datatx.state = "0";
  datatx.mode = "0";
  datatx.id=id; // Asignación de ID del dispositivo

  digitalWrite(led_green, LOW);
  digitalWrite(led_red, LOW);
  digitalWrite(led_yellow, LOW);

  tiempoActual=millis();
  tiempoAmbarDetected=millis();
  tiempoDetected=millis();

  frecuenciaParpadeo=frecuenciaParpadeo1;
}
```

### SafeDigitalWrite
Función que limita el funcionamiento de digitalWrite para ciertos casos. Se utiliza en la función cambiar_modo.

```Arduino
void safeDigitalWrite(int pin, int value) {
    if (outputEnabled || pin == led_green) {
        digitalWrite(pin, value);
    }
}
```

### States_changed
Función para cambiar el estado actual de modo. En caso el modo sea el mismo que el anterior, se mantiene el estado anterior del modo; encambio, si el modo nuevo no es igual al anterior, se actualiza el valor del modo.
```Arduino
void states_changed(String estadoActual_modo){
  if(mode_before == estadoActual_modo){
    state_change = 0;
  }else{
    mode_before = estadoActual_modo;
    state_change = 1;
  }
}
```
### InicioSistema

Función para definir la señalización de la baliza según el proceso en el que se encuentre el SMP durante su inicio a través de la bandera "flagConexionSistema".
- Estado energizado - conexión con WiFi [0]: El SMP se encuentra energizado y busca la señal WiFi definida para realizar la conexión. La baliza parpadea todas las luces con un periodo de 1 segundo hasta establecer la conexión WiFi.
- Estado conexión con Broker [1]: El SMP ha establecido la conexión WiFi. El sistema procede a establecer la conexión con el broker EMQX. El led verde de la baliza parpadea con un periodo de 3 segundos como señal de que se encuentra en dicho proceso. Nota importante: Es posible que el sistema se encuentre en este estado cuando no hay internet a pesar de que la conexión WiFi con el dispositivo proveedor (router, modem, AP u otro) haya sido realizada.

- Estado funcionamiento óptimo [2]: El SMP se encuentra en su funcionamiento óptimo para recibir información de la IA y notificar las irregularidades presentes en planta producto de la ausencia de EPPs.
```Arduino
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
```
### Cambio_modo
Función para el cambio de modo del SMP. Los modos programados son los siguientes:
- Modo Industrial [0]: El SMP utiliza la bocina industrial para notificar la sanción de un evento irregular producto de la ausencia de EPPs.
- Modo Desarrollo [1]: El SMP utiliza el buzzer para señalizar la irregularidad detectada.
- Modo Silencio [2]: El SMP desactiva el uso de alguna señal sonora y visual, a excepción de la luz verde, la cual seguirá prendida como señal de funcionamiento del SMP.


```Arduino
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
```
### Finish
Función que se encarga de la señalización visual de que se ha emitido un reporte a la CPIA. El led rojo se enciende y se envía un json al broker para indicar la finalización del conteo.

```Arduino
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

  frecuenciaParpadeo=frecuenciaParpadeo1;//Restablece el tiempo parpadeo del led amarillo al lento inicial
  flagparpadeoAmbar=0;//Restablece el flag de parpadeo ambar
  tiempoDetected=tiempoActual;
  tiempoAmbarDetected=tiempoActual;
  flagconectado=2;
  InicioSistema(flagconectado);
  Serial.print("Se terminó la funcion Finish");
}
```
### Detected
La siguiente función se encarga de realizar la señal del led ámbar en la baliza producto de la detección de un escenario correctivo de la IA (asuencia de EPP). El led ámbar parpadea en 3 velocidades como señal que el tiempo de tolerancia está por finalizar (15 segundos).

```Arduino
//Activa parpadeo led ambar por 15seg
void Detected()
{
  if (String(datatx.detection) == "1" && String(datatx.state)=="0")
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
```

#### Función principal

La siguiente es la función principal del código para controlar todas las funciones incorporadas al código.

```Arduino
if(WiFi.status() != WL_CONNECTED){ // verifica si aún existe conexión WiFi
    wifiInit();
  }else{
    if (!client.connected()){
      reconnect();
    }
    client.loop();

    tiempoActual=millis();

    Serial.println(String(datatx.detection));
    if(String(datatx.detection)=="0"){
      tiempoAmbarDetected=millis();
      tiempoDetected=millis();
      frecuenciaParpadeo=frecuenciaParpadeo1;
    }

    if ((tiempoActual - lastTime > frecuenciaPrincipal) && String(datatx.id)==id){
      Serial.println("Dentro de la lógica principal");
      Detected();
      lastTime = millis();
    }
  }
```

## Preguntas frecuentes
- Explicación del comportamiento de la baliza al iniciar el sistema:

- Definición de clientID:
Se presentaron problemas para la conexión del sistema con el broker. Anteriormente se colocaba un string como cliente, cuando este debe variar según cada dispositivo instalado debido a que naturalmente, si dos dispositivos están conectados mediante un mismo clientID, ambos se desconectan del broker.

- Problemas con el manejo temporal de funciones:


