//definimos el identificador del módulo
#define ID "estación 1"
//Definimos si usaremos gps o no (módulos sim808 y sim868)
#define USE_GPS true
//Definimos si estamos en modo depuración o no
#define DEBUG_MODE true;
//Definimos los credenciales del APN para el uso de GPRS
#define GPRS_APN    "internet"
#define GPRS_USER   NULL
#define GPRS_PASS   NULL
// Definimos el PIN de la tarjeta sim (si está activo)
	//#define GSM_PIN "4217"
//Definimos los detalles del servidor
						/*const char server[] = "postssl.meph.website";
						const char resource[] = "/?msg=sim808GET";
						const int  port = 443;*/

//pines que usaremos ( RST Y PWKEY para algunos modelos de sim808 y sim7600e )
#define PIN_RX 		8
#define PIN_TX 		9

#define PIN_PWKEY	7
#define PIN_RST		6


//Definimos los sensores que queremos leer
#define INPUT_SENSORS 2

#define PIN_SENSOR1	10
#define PIN_SENSOR2	11
//#define PIN_SENSOR3	12

//Definimos si queremos que se active un pin de salida ante una entrada concreta en el sensor 1
#define PIN_OUTPUT 13
//Definimos el valor del sensor 1 para el cual activará el PIN_OUTPUT a HIGH
#define SENSOR1_THRESHOLD 150


/*				</FIN DE LA DEFINICIÓN DE VARIABLES>				*/


#define STRLCPY_P(s1, s2) strlcpy(s1, s2, BUFFER_SIZE) // <- ATMEGA32U4

//Incluímos la clase que gestionará la librería del módulo sim
#include "Sim.h"

void setup() //configuración de arranque
{
	// configuramos los pines de salida
	#ifdef PIN_OUTPUT
		pinMode(PIN_OUTPUT, OUTPUT);
	#endif
	pinMode(PIN_PWKEY, OUTPUT);
	pinMode(PIN_RST, OUTPUT);
	
///////////// !!! CUIDAO CON ESTE RESET EN SIM800L : ( -> y en el constructor de la librería también lo hace! )
	
	//#if USE_GPS == true	
		digitalWrite(PIN_PWKEY, LOW);
		digitalWrite(PIN_RST, HIGH);
	//#endif	

}


void loop()
{
	char current_time[20]; //almacenamos el tiempo de adquisición de los sensores
	boolean gps_position_found;
	boolean http_post_correct;
	
	Sim mysim(USE_GPS,DEBUG_MODE);
	
	delay(120000); // dos minutos de espera para que coja gps
	
	/*módulo arrancado, conectado a red y a gps*/
	
	while( mysim.is_full_connected() )	//hasta que no se vuelva a verificar la conexión, se mantendrá dentro del bucle para no resetear el módulo al inicializar la clase Sim.
	{

		//if data_waiting_in_sd
			//bucle read_sd_data_row
				//send_http_post(sd_data_row)


		
	//Toma los valores de los sensores
		for(i=0; i<INPUT_SENSORS; i++)
		{
			sensor_value[i]=get_sensor_value(i);
			
			#ifdef PIN_OUTPUT
				if(i==0 && sensor_value[i]>SENSOR1_THRESHOLD)
				{
					digitalWrite(PIN_OUTPUT, HIGH);
				} else
					digitalWrite(PIN_OUTPUT, LOW);					
			#endif
		}
			
	//Toma el tiempo actual del reloj del módulo sim
		mysim.getInternalClock(&current_time);
		
	//Toma la posición gps
		if(USE_GPS)
		{
			gps_position_found=false;
			for(int i=0; i<6 || gps_position_found; i++)//LOOP de 6 intentos con delays de 120/6 segundos (1 minuto aprox. para encontrar gps's)
			{
				gps_position_found=mysim.getGPS();
				delay(120000/6);
			}
		}
	
	//Envía los datos obtenidos de los sensores
		for(sensor_number=1; i<INPUT_SENSORS+1; sensor_number++)
		{
			//Si encontró la posición gps o si no usa gps envía la petición http_post
			if( (USE_GPS && gps_position_found) || !USE_GPS )
				http_post_correct = mysim.send_http_post(ID, sensor_number, sensor_value[sensor_number], time );
			
			//si no encontró la posición gps, o la petición post devolvió error: escribe los datos del sensor a la tarjeta sd)
			if( (USE_GPS && !gps_position_found) || !http_post_correct )
				mysim.save_in_sd_card(ID, sensor_number, sensor_value[sensor_number], time);
		}

		mysim.gprsDisconnect();
		if(DEBUG_MODE)Serial.println(F("GPRS disconnected"));

	}

}
