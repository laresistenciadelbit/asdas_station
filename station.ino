/*	-Proyecto:	Arduino Sim Data Adquisition System 
	-Autor:		Noel Fernández Peral
	-Requisitos:
		<>Hardware:
			[]Arduino pro micro (Compatible con otras variantes)
			[]Sim8xx (Cualquier variante de sim800 es compatible)
*/
	
	//definimos el identificador del módulo
#define ID "estación 1"
	//definimos una contraseña para las peticiones
#define PASSWD "1AeIoU!9"
	//Definimos si usaremos gps o no (módulos sim808 y sim868)
#define USE_GPS 0
	//Definimos cuantas estadísticas queremos enviar (sin contar la localización gps)
#define SEND_STATUS 1
	//Definimos si estamos en modo depuración o no
#define DEBUG_MODE true
	//Definimos el código pin si la tarjeta lo tiene activo
//#define UNLOCK_PIN "0000";
	//Definimos los credenciales del APN para el uso de GPRS
#define GPRS_APN    "internet"
#define GPRS_USER   NULL
#define GPRS_PASS   NULL
	//pines que usaremos (pines RX y TX dedicados)( RST Y PWKEY para algunos modelos de sim808 y sim7600e )
#define PIN_RX 		8
#define PIN_TX 		9

#define PIN_PWKEY	7
#define PIN_RST		6

  //Definimos los pines de los sensores que queremos leer (podemos poner tantos como queramos)
const int8_t sensors_pin[]={10,11};
const int8_t sensors_name[]={"temperature","co2"};
	//Definimos si queremos que se active un pin de salida ante una entrada concreta en el sensor 1
#define PIN_OUTPUT 13
	//Definimos el valor del sensor 1 para el cual activará el PIN_OUTPUT a HIGH
#define SENSOR1_THRESHOLD 150
  //Definimos un timeout si queremos que el pin de output se desactive tras ese tiempo (ms)
#define OUTPUT_TIMEOUT 6000

	//Definimos la dirección del servidor que recebirá las peticiones
#define SERVER_ADDRESS "http://post.diab.website"

	//definimos un límite de caracteres para el envío de estado
#define STATUS_CHAR_LIMIT 14

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

  for(int8_t i=0; i<sizeof(sensors_pin); i++)
    pinMode(sensors_pin[i], INPUT);
	
///////////// !!! CUIDAO CON ESTE RESET EN SIM800L : ( -> y en el constructor de la librería también lo hace! )
	//#if USE_GPS == true	
		digitalWrite(PIN_PWKEY, LOW);
		digitalWrite(PIN_RST, HIGH);
	//#endif
}

/*
	Rellena los vectores de estado
	i->contador
	a->vector destino
	aa->cadena
	b->vector destino
	bb->cadena
*/
void build_status_array(uint8_t *i, char **a, char *aa, char **b, char *bb)
{
	strlcpy(a[i],aa,sizeof(a[i]));
	strlcpy(b[i],bb,sizeof(b[i]));
	*i++;
}

void loop()
{
	bool http_post_correct;
	uint8_t sensor_value[sizeof(sensors_pin)];
	uint8_t sensor_number_str_buffer[2]; //pondremos un valor máximo de sensores de 2 cifras (99 sin contar el 0), a pesar de estar limitados a los pines que nos sobren del micro.
	uint8_t sensor_value_str_buffer[4];  //leemos el sensor con resolución de 10 bits, como máximo será un valor de 1024 (4 caracteres)
	char sim_unlock_pin[4]={NULL,NULL,NULL,NULL};
	#ifdef UNLOCK_PIN
		strlcpy(sim_unlock_pin,UNLOCK_PIN,sizeof(4);
	#endif

	char status_name[SEND_STATUS][STATUS_CHAR_LIMIT];
	char status_data[SEND_STATUS][STATUS_CHAR_LIMIT];
	uint8_t total_status;
	uint8_t current_status;
	char[3] satellites; 	//opcional
	char[3] battery; 		//opcional
	
	ModuleManager ASDAS(ID,SERVER_ADDRESS,USE_GPS,DEBUG_MODE,PIN_PWKEY,PIN_RST,sim_unlock_pin,GPRS_APN,GPRS_USER,GPRS_PASS);
	//Sim ASDAS(SERVER_ADDRESS,USE_GPS,DEBUG_MODE, UNLOCK_PIN); // <- si tenemos código pin configurado en la tarjeta, usaremos esta inicialización de clase
	
	if(USE_GPS)
		delay(120000); // dos minutos de espera para que coja señal gps
	
	/*módulo arrancado, conectado a red y a gps*/
	
	while( ASDAS.is_full_connected() )	//hasta que no se vuelva a verificar la conexión, se mantendrá dentro del bucle para no resetear el módulo al inicializar la clase Sim.
	{

		while( ASDAS.data_waiting_in_sd() )	//mientras haya líneas en el fichero de almacenamiento
		{
			if(!ASDAS.send_last_sd_line())	//las envía, si falla al enviar se sale
				break;
		}

	//Toma los valores de los sensores
		for(uint8_t i=0; i<sizeof(sensors_pin); i++)
		{
			sensor_value[i]=ASDAS.get_sensor_value(sensors_pin[i]);
			
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
		ASDAS.get_time();
		
	//Toma la posición gps
		if(USE_GPS)
			ASDAS.get_gps();
	
	//Envía los datos obtenidos de los sensores
		http_post_correct=true;	//la tomamos por buena desde el principio para que quede como falsa en cualquiera de las peticiones que llegasen a dar error en toda la iteración.
		for(uint8_t sensor_number=0; sensor_number<sizeof(sensors_pin); sensor_number++)
		{
		//convertimos en cadena de caracteres el valor del sensor				<a--- meterlo para devolver en una función en la clase Sensores
			itoa(sensor_value[sensor_number],sensor_value_str_buffer,10);

		//envía la petición http_post
			if( !ASDAS.send_data_to_server(sensor_name[sensor_number], sensor_value_str_buffer ) )
			{
				ASDAS.save_data_in_sd(sensor_name[sensor_number], sensor_value_str_buffer);	//si la petición post devolvió error: escribe los datos del sensor a la tarjeta sd
				http_post_correct = false;
			}
		}
		if( (SEND_STATUS || USE_GPS) )
		{
			if (SEND_STATUS) //aquí rellenamos el array de status_data si queremos mandar más datos adicionales (porcentaje de batería, satélites detectados, errores...)
			{
				current_status=0;
				//OPCIONAL (estatus de la batería)
				itoa( ASDAS.battery_left(), battery, 10 );
				build_status_array( &current_status, status_name, "battery" , status_data, battery);
				
				/* 
				
				//OPCIONAL (satélites encontrados)

				itoa( ASDAS.get_satellites_found(), satellites, 10 );
				build_status_array( &current_status,  status_name, "satellites", status_data, satellites  );
			
				//OPCIONAL (error encontrado al activar un flag)
			
				if(flag_error_example1 || flag_error_example2 || flag_error_example3 )
				{
					build_status_array( &current_status, status_name, "error", status_data, error_content );
				}
				*/
				
				if(USE_GPS)
				{
					build_status_array( &current_status, status_name,"lat", status_data, "" );
					build_status_array( &current_status, status_name,"lon", status_data, "" );
				}
			}
			
			//total_status=SEND_STATUS+USE_GPS;
			total_status=current_status+USE_GPS;	//<- por seguridad no usaremos la constante de SEND_STATUS para contar los parámetros a enviar sino los sumados al recorrer el código de parámetros activados de estado
			
			for( uint8_t status_count=0; status_count<total_status; status_count++ )
			{
				if(http_post_correct)
				{
					if( !ASDAS.send_aditional_data_to_server(status_name[status_count],status_data[status_count]) )
						ASDAS.save_aditional_data_in_sd(status_name[status_count],status_data[status_count]);
				}
				else
					ASDAS.save_aditional_data_in_sd(status_name[status_count],status_data[status_count]);
			}
		}

		delay(60);	//cambiar por una espera medida
		
// !!! APAGAMOS (gps y/o gprs) ??? ->>>>>>
//		ASDAS.gprsDisconnect();
//    ASDAS.gpsDisconnect();
//		if(DEBUG_MODE)Serial.println(F("Radios disconnected"));

// !!! PONEMOS EN MODO AHORRO DE ENERGÍA ??? 

// !!! DORMIMOS

	}

}
