	//definimos el identificador del módulo
#define ID "estación 1"
	//Definimos si usaremos gps o no (módulos sim808 y sim868)
#define USE_GPS true
	//Definimos si estamos en modo depuración o no
#define DEBUG_MODE true
	//Definimos el código pin si la tarjeta lo tiene activo
//#define UNLOCK_PIN "0000";
	//Definimos los credenciales del APN para el uso de GPRS
#define GPRS_APN    "internet"
#define GPRS_USER   NULL
#define GPRS_PASS   NULL
	//pines que usaremos ( RST Y PWKEY para algunos modelos de sim808 y sim7600e )
#define PIN_RX 		8
#define PIN_TX 		9
// Arduino no permite instanciar clases, por lo que la definición de estos pines también está en el fichero "ModuleManager.h"
#define PIN_PWKEY	7
#define PIN_RST		6

  //Definimos los pines de los sensores que queremos leer (podemos poner tantos como queramos)
const int8_t pin_sensors[]={10,11};

	//Definimos si queremos que se active un pin de salida ante una entrada concreta en el sensor 1
#define PIN_OUTPUT 13
	//Definimos el valor del sensor 1 para el cual activará el PIN_OUTPUT a HIGH
#define SENSOR1_THRESHOLD 150
  //Definimos un timeout si queremos que el pin de output se desactive tras ese tiempo (ms)
#define OUTPUT_TIMEOUT 6000

	//Definimos la dirección del servidor que recebirá las peticiones
#define SERVER_ADDRESS "http://post.meph.website"

/*				</FIN DE LA DEFINICIÓN DE VARIABLES>				*/

//#define STRLCPY_P(s1, s2) strlcpy(s1, s2, BUFFER_SIZE) // <- ATMEGA32U4

//Incluímos la clase que gestionará la librería del módulo sim
#include "ModuleManager.h"

void setup() //configuración de arranque
{
	// configuramos los pines de salida
	#ifdef PIN_OUTPUT
		pinMode(PIN_OUTPUT, OUTPUT);
	#endif
	pinMode(PIN_PWKEY, OUTPUT);
	pinMode(PIN_RST, OUTPUT);

  for(int8_t i=0; i<sizeof(pin_sensors); i++)
    pinMode(pin_sensors[i], INPUT);
	
///////////// !!! CUIDAO CON ESTE RESET EN SIM800L : ( -> y en el constructor de la librería también lo hace! )
	//#if USE_GPS == true	
		digitalWrite(PIN_PWKEY, LOW);
		digitalWrite(PIN_RST, HIGH);
	//#endif

}


void loop()
{
	char current_time[20]; //almacenamos el tiempo de adquisición de los sensores
	bool gps_position_found;
	bool http_post_correct;
  uint8_t sensor_value[sizeof(pin_sensors)];
  uint8_t sensor_number_str_buffer[2]; //pondremos un valor máximo de sensores de 2 cifras (99 sin contar el 0), a pesar de estar limitados a los pines que nos sobren del micro.
  uint8_t sensor_value_str_buffer[4];  //leemos el sensor con resolución de 10 bits, como máximo será un valor de 1024 (4 caracteres)
  char sim_unlock_pin[4]={NULL,NULL,NULL,NULL};
  #ifdef UNLOCK_PIN
    strlcpy(sim_unlock_pin,UNLOCK_PIN,sizeof(4);
  #endif
	
	ModuleManager mysim(SERVER_ADDRESS,USE_GPS,DEBUG_MODE,PIN_PWKEY,PIN_RST,sim_unlock_pin,GPRS_APN,GPRS_USER,GPRS_PASS);
	//Sim mysim(SERVER_ADDRESS,USE_GPS,DEBUG_MODE, UNLOCK_PIN); // <- si tenemos código pin configurado en la tarjeta, usaremos esta inicialización de clase
	
	delay(120000); // dos minutos de espera para que coja gps
	
	/*módulo arrancado, conectado a red y a gps*/
	
	while( mysim.is_full_connected() )	//hasta que no se vuelva a verificar la conexión, se mantendrá dentro del bucle para no resetear el módulo al inicializar la clase Sim.
	{

		while( mysim.data_waiting_in_sd() )	//mientras haya líneas en el fichero de almacenamiento
		{
			if(!mysim.send_last_sd_line())	//las envía, si falla al enviar se sale
				break;
		}

	//Toma los valores de los sensores
		for(uint8_t i=0; i<sizeof(pin_sensors); i++)
		{
			sensor_value[i]=mysim.get_sensor_value(pin_sensors[i]);
			
			#ifdef PIN_OUTPUT
				if(i==0 && sensor_value[i]>SENSOR1_THRESHOLD)
				{
					digitalWrite(PIN_OUTPUT, HIGH); //superó el valor límite el sensor1: activamos la salida por el pin de salida
         
          #ifdef OUTPUT_TIMEOUT //la desactivamos tras un tiempo si hemos definido el timeout
            delay(OUTPUT_TIMEOUT);
            digitalWrite(PIN_OUTPUT, LOW);
          #endif
          
				} else
					digitalWrite(PIN_OUTPUT, LOW);					
			#endif
		}
			
	//Toma el tiempo actual del reloj del módulo sim
		mysim.get_time(current_time);
		
	//Toma la posición gps
		if(USE_GPS)
		{
			gps_position_found=false;
			for(int i=0; i<6 || gps_position_found; i++)//LOOP de 6 intentos con delays de 120/6 segundos (1 minuto aprox. para encontrar gps's)
			{
				gps_position_found=mysim.get_gps();
				delay(120000/6);
			}
		}
	
	//Envía los datos obtenidos de los sensores
		for(uint8_t sensor_number=1; sensor_number<sizeof(pin_sensors)+1; sensor_number++)
		{
			//convertimos en cadena de caracteres tanto el número del sensor como su valor
			itoa(sensor_number,sensor_number_str_buffer,10);
			itoa(sensor_value[sensor_number],sensor_value_str_buffer,10);
    
			//Si encontró la posición gps o si no usa gps envía la petición http_post
			if( (USE_GPS && gps_position_found) || !USE_GPS )
				http_post_correct = mysim.send_data_to_server(ID, sensor_number_str_buffer, sensor_value_str_buffer, current_time );
			
			//si no encontró la posición gps, o la petición post devolvió error: escribe los datos del sensor a la tarjeta sd)
			if( (USE_GPS && !gps_position_found) || !http_post_correct )
			{
				mysim.save_in_sd_card(ID, sensor_number_str_buffer, sensor_value_str_buffer, current_time);
			}
		}

// !!! APAGAMOS (gps y/o gprs) ??? ->>>>>>
//		mysim.gprsDisconnect();
//    mysim.gpsDisconnect();
//		if(DEBUG_MODE)Serial.println(F("Radios disconnected"));

// !!! PONEMOS EN MODO AHORRO DE ENERGÍA ??? 

// !!! DORMIMOS

	}

}
