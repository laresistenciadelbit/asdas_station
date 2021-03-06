/*	-Proyecto:	Arduino Sim Data Acquisition System 
	-Autor:		Noel Fernández Peral
	-Requisitos:
		<>Hardware:
			[]Arduino pro micro (Compatible con otras variantes)
			[]Módulo Sim8xx (Cualquier variante de sim800 es compatible)
			[]Módulo de lectura/escritura de tarjeta micro SD
*/
	//definimos el identificador del módulo
#define ID "estación 1"
	//definimos una contraseña para las peticiones
#define PASSWD "1AeIoU!9"
	//Definimos si usaremos gps o no (módulos sim808 y sim868)
#define USE_GPS  1
	//Definimos cuantas estadísticas queremos enviar (sin contar la localización gps)
#define SEND_STATUS 1
	//Definimos si estamos en modo depuración o no
//#define DEBUGGING false // <-- dentro de la clase ModuleManager, ya que el pasarsela como constante asignando a una variable hacía que precompilase el código de debug aunque no se usase
	//Definimos el código pin si la tarjeta lo tiene activo
//#define UNLOCK_PIN "0000"
	//Definimos los credenciales del APN para el uso de GPRS
#define GPRS_APN "internet"
#define GPRS_USER   ""
#define GPRS_PASS   ""
	//pines que usaremos (pines RX y TX dedicados)( RST Y PWKEY para algunos modelos de sim808 y sim7600e )
//#define PIN_RX 		0   USAMOS Serial1 que es puerto de serie por hardware, siempre són los pines 0 y 1 (RXI y TX0) en arduino pro micro
//#define PIN_TX 		1

#define  PIN_RST   7
#define  PIN_PWKEY 6

  //Definimos los pines de los sensores que queremos leer (podemos poner tantos como queramos. *el límite son los pines y la memoria)
#define  SENSOR_NUMBER 2
const uint8_t  sensor_pin[SENSOR_NUMBER]={8,9};
const char  sensor_name[][30]={"temperatura","CO"};
	//Definimos si queremos que se active un pin de salida ante una entrada concreta en el sensor 1
//#define PIN_OUTPUT 13
	//Definimos el valor del sensor 1 para el cual activará el PIN_OUTPUT a HIGH
#define SENSOR1_THRESHOLD 150
  //Definimos un timeout si queremos que el pin de output se desactive tras ese tiempo (ms)
#define OUTPUT_TIMEOUT 6000

	//Definimos la dirección del servidor que recebirá las peticiones
#define  SERVER_ADDRESS "http://post.diab.website"
	//definimos un límite de caracteres para el envío de estado
#define STATUS_CHAR_LIMIT 14

	//Definimos cada cuantos minutos leerá y mandará la información de los sensores
#define WORKING_INTERVAL  5
	//Definimos cuántas veces leerá e intentará enviar la información antes de resetear el microcontrolador por incapacidad de conexión para que reinicie todos los módulos
#define SIM_TIMEOUT_THRESHOLD 5
/*				</FIN DE LA CONFIGURACIÓN DE VARIABLES>				*/

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

	for(int8_t i=0; i<SENSOR_NUMBER; i++)
		pinMode(sensor_pin[i], INPUT);
	
///////////// Reset para SIM808 y SIM868 (no usado)
	//#if USE_GPS == true	
//		digitalWrite(PIN_PWKEY, LOW);
//		digitalWrite(PIN_RST, HIGH);
	//#endif
  
	#if DEBUGGING == false	//si no hay debugging por puerto de serie usamos el led de RX como indicador
		pinMode(17, OUTPUT);
	#endif
}

/*  build_status_array():
		Rellena los vectores de datos de estado
			i->contador
			a->vector destino
			aa->cadena
			b->vector destino
			bb->cadena
*/
void build_status_array(uint8_t *i, char a[][STATUS_CHAR_LIMIT], char *aa, char b[][STATUS_CHAR_LIMIT], char *bb)
{
	strlcpy(a[*i],aa,sizeof(a[*i]));
	strlcpy(b[*i],bb,sizeof(b[*i]));
	*i=*i+1;
}

void loop()
{
	bool http_post_correct;
	uint8_t sensor_value[SENSOR_NUMBER];
	char sensor_value_str_buffer[4];  //leemos el sensor con resolución de 10 bits, como máximo será un valor de 1024 (4 caracteres)
	char sim_unlock_pin[4]={NULL,NULL,NULL,NULL};
	#ifdef UNLOCK_PIN
		strlcpy(sim_unlock_pin,UNLOCK_PIN,sizeof(4));
	#endif

	char status_name[SEND_STATUS+2][STATUS_CHAR_LIMIT]; //+2 del gps (latitud,longitud)
	char status_data[SEND_STATUS+2][STATUS_CHAR_LIMIT];
	uint8_t total_status;
	char satellites[3]; 	//opcional
	char battery[3]; 		//opcional
	
	uint8_t sim_timeout_counter=0;
	bool reset_modules=false;

	unsigned long working_interval_mili=WORKING_INTERVAL*60L*1000L;
	unsigned long previousMillis = millis();
	unsigned long currentMillis = previousMillis; //las inicializamos igual puesto que arduino puede reiniciar el loop sin reinicializar los contadores

	ModuleManager ASDAS(PASSWD,ID,SERVER_ADDRESS,USE_GPS,PIN_PWKEY,PIN_RST,sim_unlock_pin,GPRS_APN,GPRS_USER,GPRS_PASS);
	//Sim ASDAS(SERVER_ADDRESS,USE_GPS,DEBUGGING, UNLOCK_PIN); // <- si tenemos código pin configurado en la tarjeta, usaremos esta inicialización de clase
	#if DEBUGGING==false
		digitalWrite(17, LOW);delay(200);digitalWrite(17, HIGH);delay(200);digitalWrite(17, LOW);delay(200);digitalWrite(17, HIGH);delay(200);digitalWrite(17, LOW);delay(200);digitalWrite(17, HIGH);delay(200);digitalWrite(17, LOW);delay(200);digitalWrite(17, HIGH);delay(200);
	#endif

	if(USE_GPS)
	{	// dos minutos de espera al arranque para que coja señal gps
		digitalWrite(17, LOW);delay(30000);digitalWrite(17, HIGH);delay(30000);
		digitalWrite(17, LOW);delay(30000);digitalWrite(17, HIGH);delay(30000);
		#if DEBUGGING == true
			Serial.println(F("Waiting 2 minutes for the gps signal..."));
		#endif
	}
 
	/*módulo arrancado, conectado a red y a gps*/
	if(ASDAS.is_full_connected())
	{
		#if DEBUGGING == true
			  Serial.println(F("[+]Connected to 2g"));
		#endif
		ASDAS.prevention_delay(); //damos 3 segundos de estabilización
	}
	
	while(!reset_modules)
	{
		#if DEBUGGING == true
			  Serial.println(F("Reading sensors..."));
		#endif
		//Toma los valores de los sensores
		for(uint8_t i=0; i<SENSOR_NUMBER; i++)
		{
			sensor_value[i]=ASDAS.get_sensor_val(sensor_pin[i]);

			#if DEBUGGING == true
			Serial.println(F("")); Serial.print(F("[+]Sensor: ")); Serial.print(sensor_name[i]); Serial.print(F(" value: ")); Serial.print(sensor_value[i]); Serial.println(F(""));
			#endif
			
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
		#if DEBUGGING == true
			if(USE_GPS)
				Serial.println(F("Obtaining GPS location..."));
		#endif

		if(USE_GPS)
			ASDAS.get_gps();

		#if DEBUGGING == true
			Serial.println(F("Looking for data in the SD buffer..."));
		#endif
			
		if( ASDAS.is_full_connected() )
		{
			while( ASDAS.data_waiting_in_sd() )	//mientras haya líneas en el fichero de almacenamiento
			{
				if(!ASDAS.send_last_sd_line())	//las envía, si falla al enviar se sale
					break;
				ASDAS.prevention_delay();
			}
		//Envía los datos obtenidos de los sensores
			#if DEBUGGING == true
				Serial.println(F("Sending sensor data..."));
			#endif
			http_post_correct=true;	//la tomamos por buena desde el principio para que quede como falsa en cualquiera de las peticiones que llegasen a dar error en toda la iteración.
			for(uint8_t sensor_number=0; sensor_number<SENSOR_NUMBER; sensor_number++)
			{
			//convertimos en cadena de caracteres el valor del sensor				<a--- meterlo para devolver en una función en la clase Sensores
				itoa(sensor_value[sensor_number],sensor_value_str_buffer,10);

			//envía la petición http_post
				if( !ASDAS.send_data_to_server(sensor_name[sensor_number], sensor_value_str_buffer ) )
				{
					ASDAS.save_data_in_sd(sensor_name[sensor_number], sensor_value_str_buffer);	//si la petición post devolvió error: escribe los datos del sensor a la tarjeta sd
					http_post_correct = false;
				}
				ASDAS.prevention_delay();
			}

		//Envía los estados y ubicación
			#if DEBUGGING == true
				Serial.println(F("Sending status data..."));
			#endif
			if( (SEND_STATUS || USE_GPS) )
			{
				total_status=0;
		  
				if (SEND_STATUS) //aquí rellenamos el array de status_data si queremos mandar más datos adicionales (porcentaje de batería, satélites detectados, errores...)
				{
					  //OPCIONAL (estado de la batería)
					itoa( ASDAS.battery_left(), battery, 10 );
					build_status_array( &total_status, status_name, "battery" , status_data, battery);

					/* 
					  //OPCIONAL (satélites encontrados)
					itoa( ASDAS.get_satellites_found(), satellites, 10 );
					build_status_array( &total_status,  status_name, "satellites", status_data, satellites  );
				
					  //OPCIONAL (error encontrado al activar un flag)
					if(flag_error_example1 || flag_error_example2 || flag_error_example3 )
					{
						build_status_array( &total_status, status_name, "error", status_data, error_content );
					}
					*/
				}
				if( USE_GPS && ASDAS.got_gps_position() )
				{
					build_status_array( &total_status, status_name, "lat", status_data, "" );
					build_status_array( &total_status, status_name, "lon", status_data, "" );
				}
				
				for( uint8_t status_count=0; status_count<total_status; status_count++ )
				{
					if(http_post_correct)
					{
						http_post_correct = ASDAS.send_aditional_data_to_server(status_name[status_count],status_data[status_count]);
						ASDAS.prevention_delay();
						if( !http_post_correct )
							ASDAS.save_aditional_data_in_sd(status_name[status_count],status_data[status_count]);
					}
					else
						ASDAS.save_aditional_data_in_sd(status_name[status_count],status_data[status_count]);
           
					ASDAS.prevention_delay();
				}
			}
		}

		#if DEBUGGING == true
			Serial.print(F("\n * * * END!!! - Waiting ")); Serial.print(WORKING_INTERVAL); Serial.print(F("minutes...\n"));
		#endif
		
		// !!! APAGAMOS (gps y/o gprs) ??? ->>>>>>
		//		ASDAS.gprsDisconnect();
		//    	ASDAS.gpsDisconnect();
		//		if(DEBUGGING)Serial.println(F("Radios disconnected"));
		// !!! PONEMOS EN MODO AHORRO DE ENERGÍA ??? 

		while (currentMillis - previousMillis < working_interval_mili)  //se mantiene en el bucle hasta que pase el tiempo de espera
		{
		  #if DEBUGGING == true
			Serial.println(F("waiting..."));
		  #endif
		  delay(1000);
		  currentMillis = millis();
		}

		previousMillis = currentMillis; //reseteamos el contador
			
		if( !ASDAS.is_full_connected() || !http_post_correct ) //si no se consiguió conectar o falló al enviar alguna petición web
		{
			if(sim_timeout_counter == SIM_TIMEOUT_THRESHOLD)
			{
				sim_timeout_counter=0;
				reset_modules=true;
			}
			else
				sim_timeout_counter++;
		}
		if(http_post_correct)
			sim_timeout_counter=0;
	}
}
