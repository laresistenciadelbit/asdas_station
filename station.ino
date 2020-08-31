//->! probar con GET primero en ambos módulos
//	después probar con post usando:
//		https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
//		https://github.com/vshymanskyy/TinyGSM/issues/130

#define PIN_RX 		8
#define PIN_TX 		9
#define PIN_RST		6
#define PIN_PWKEY	7
		
		
// Select your modem:
//#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM808

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
//#define SerialAT Serial1

// or Software Serial on Uno, Nano
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(PIN_RX, PIN_TX); // RX, TX
//->! probablemente tengamos que poner resistencia entre pwrin y gnd para que funione

// Increase RX buffer to capture the entire response
// Chips without internal buffering (A6/A7, ESP8266, M590)
// need enough space in the buffer for the entire response
// else data will be lost (and the http library will fail).
#if !defined(TINY_GSM_RX_BUFFER)
  #define TINY_GSM_RX_BUFFER 700 //650
#endif

// See all AT commands, if wanted <-- DESACTIVAR PARA MODO NO DEBUG !!! (usa mas espacio)
#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon
// #define LOGGING  // <- Logging is for the HTTP library

// Range to attempt to autobaud
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 9600 //115200 disminuido a 9600 ya que nuestro módulo va a usar siempre 9600 para un correcto funcionamiento

// Add a reception delay - may be needed for a fast processor at a slow baud rate
// #define TINY_GSM_YIELD() { delay(2); }

// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true

#define TINY_GSM_SSL_CLIENT_AUTHENTICATION  //añadido en la última versión de la librería, fuerza la conexión ssl

// set GSM PIN, if any
#define GSM_PIN "4217"

// Your GPRS credentials, if any
const char apn[]  = "internet";
//const char gprsUser[] = "";
//const char gprsPass[] = "";

// Server details
const char server[] = "postssl.meph.website";
const char resource[] = "/?msg=sim808GET";
const int  port = 443;

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGsmClientSecure client(modem);
HttpClient http(client, server, port);

void setup() {
  // Set console baud rate
  SerialMon.begin(38400);
  delay(10);

// Set your reset, enable, power pins here

pinMode(PIN_PWKEY, OUTPUT);
pinMode(PIN_RST, OUTPUT);
digitalWrite(PIN_PWKEY, LOW);
digitalWrite(PIN_RST, HIGH);

//	->! si no funciona, el pwkey y el rst funcionan de forma diferente.
	//si no va, probar a usar comentandolo, si no va, probar a poner resistencia entre pwkey y gnd


  SerialMon.println("Wait...");

  // Set GSM module baud rate
  //TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
   SerialAT.begin(9600);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // modem.init();
delay(2000);

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);
delay(2000);

}

void loop() {

    // Unlock your SIM card with a PIN if needed
    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
      modem.simUnlock(GSM_PIN);
    }

  delay(6000);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(8000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }


  // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    SerialMon.println("");
    if (!modem.gprsConnect(apn)) //, gprsUser, gprsPass)) 
    {
      SerialMon.println(" fail");
      delay(8000);

      //otro intento antes de reiniciar:
      if ( GSM_PIN && modem.getSimStatus() != 3 )
        modem.simUnlock(GSM_PIN);
      delay(6000);

      if (!modem.gprsConnect(apn, gprsUser, gprsPass))
        return;
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected()) {
      SerialMon.println("GPRS connected");
    }


  SerialMon.print(F("Performing HTTPS GET request... "));
    http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  int err = http.get(resource);
  if (err != 0) {
    SerialMon.println(F("failed to connect"));
    delay(8000);
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  // Shutdown

  http.stop();
  SerialMon.println(F("Server disconnected"));

#if TINY_GSM_USE_GPRS
    modem.gprsDisconnect();
    SerialMon.println(F("GPRS disconnected"));
#endif

  // Do nothing forever
  while (1==1);
}
