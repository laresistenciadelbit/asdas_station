//pines que usaremos ( RST Y PWKEY para algunos modelos de sim808 y sim7600e )

#include "Sim.h"

#define PIN_RX 		8
#define PIN_TX 		9
#define PIN_RST		6
#define PIN_PWKEY	7

#define PIN_SENSOR	10

// Detalles del servidor
const char server[] = "postssl.meph.website";
const char resource[] = "/?msg=sim808GET";
const int  port = 443;

// Definimos los credenciales del APN para el uso de GPRS
const char apn[]  = "internet";
//const char gprsUser[] = "";
//const char gprsPass[] = "";

// Definimos el PIN de la tarjeta sim (si está activo)
	//#define GSM_PIN "4217"

// Definimos el modem que usaremos:
	//#define TINY_GSM_MODEM_SIM7000
	//#define TINY_GSM_MODEM_SIM868
	//#define TINY_GSM_MODEM_SIM800
	#define TINY_GSM_MODEM_SIM808

#define TINY_GSM_SSL_CLIENT_AUTHENTICATION  //añadido en la última versión de la librería, fuerza la conexión ssl
#define USE_HTTP_POST false 	//(para usar POST o GET en las peticiones http, por defecto GET)


/*				</FIN DE LA DEFINICIÓN DE VARIABLES>				*/


#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

void setup() //configuración de arranque
{
	// configuramos los pines de salida
	#if defined(PIN_PWKEY)
	pinMode(PIN_PWKEY, OUTPUT);
	#endif
	#if defined(PIN_RST)
	pinMode(PIN_RST, OUTPUT);
	#endif	

	Serial.begin(38400); //iniciamos el puerto de comunicación del microcontrolador (recomendado >9600 y <115200)
	delay(10);
	
}



void loop() 
{

	Sim mysim(true);//(PIN_RX,PIN_TX);

	delay(6000);

	


    modem.gprsDisconnect();
    Serial.println(F("GPRS disconnected"));


  // Do nothing forever
  while (1==1);
}
