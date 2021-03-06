#include <SPI.h>
#include "SdFat.h"
#include <SIM8xx.h>

#include "ModuleManager.h"

ModuleManager::ModuleManager(const char* passwd, const char *id, const char *server, const bool gps, const int8_t pwkey, const int8_t rst ,char *pin, char *apn, char *apn_user, char *apn_pass)
{
	strcpy(this->station_id,id);
	strcpy(this->server_address,server);
	strcat(this->server_address,"/?pw=");
	strcat(this->server_address,passwd);

	strcpy(this->gprs_apn,apn);
	if(apn_user[0]!=0)
	{
		strcpy(this->gprs_user,apn_user);
		strcpy(this->gprs_pass,apn_pass);
	} else {
    this->gprs_user[0]=0;
    this->gprs_pass[0]=0;
	}
  
	if(pin[0]!=0)
	{
		strlcpy(this->sim_pin,pin,4);
	}
	else
		this->sim_pin[0]=0;

//Inicializamos comunicación con el pc solo para debugging
  #if DEBUGGING == true
    Serial.begin(115200); //iniciamos el puerto de comunicación del microcontrolador (recomendado >9600 y <115200)
    while (!Serial) {;}
    prevention_delay();
    Serial.println(F("Loading ModuleManager..."));
    Serial.println(this->server_address);
  #endif

// Inicializamos módulos

	// Inicializamos módulo micro SD
	if (!SD.begin(10))  //Pin ss está definido como pin 10
	{
		#if DEBUGGING == true
			Serial.println(F("[-]SD initialization failed!"));
		#endif
	}
	else
	{
		#if DEBUGGING == true
			Serial.println(F("[+]SD initialization done."));
		#endif
	}
  
//Inicializamos comunicación en serie desde microcontrolador a módulo sim
	Serial1.begin(9600);	//iniciamos la vía de comunicación con el módulo sim
	
	#if DEBUGGING == true
		Serial.println(F("Starting Sim8xx..."));
	#endif
      
	prevention_delay();
	this->sim_module = new SIM8xx(rst,pwkey);//SIM_RST, SIM_PWR);   // <- arduino no permite instanciar clases, por lo que creamos la clase SIM8xx y la inicializamos directamente desde la definicion de esta clase (en ModuleManager.h)
	this->sim_module->begin(Serial1);
	this->sim_module->powerOnOff(true);
	this->sim_module->init(); 
	
	if (gps)
	{
		this->use_gps=true;
		sim_module->powerOnOffGps(true);
	}
	else
		this->use_gps=false;
    
	if( connect_network(0) )
	{
		this->connection_successful=connect_gprs(0);
	}
	else
		this->connection_successful=false;
}

bool ModuleManager::is_full_connected(void){return this->connection_successful;}

bool ModuleManager::connect_network(uint8_t intento=0)
{
	SIM8xxNetworkRegistrationState network_status;
	bool return_status;
	
	this->unlock_sim();

	#if DEBUGGING == true
		Serial.println(S_F("[*]Waiting for network..."));
	#endif
	
	network_status = sim_module->getNetworkRegistrationStatus();
	if( !(static_cast<int8_t>(network_status) & (static_cast<int8_t>(SIM8xxNetworkRegistrationState::Registered) | static_cast<int8_t>(SIM8xxNetworkRegistrationState::Roaming)) != 0) )
	{
		#if DEBUGGING == true
			Serial.println(S_F(" [-]Fail"));
		#endif
    
		if(intento>4)
			return_status=false;//resetSoftware();
		else
		{
			delay(12500);
			return_status=connect_network(intento+1);	//recursivo
		}
	}
	else
	{
		#if DEBUGGING == true
			Serial.println(S_F("[+]Network connected"));
		#endif
		return_status=true;
	}
	return return_status;
}

bool ModuleManager::connect_gprs(uint8_t intento=0)
{
	bool return_status;
	
	this->unlock_sim(); //solo desbloquea en caso de que no se haya desbloqueado (que ya debería estarlo)
	
	#if DEBUGGING == true
		Serial.println(S_F("[*]Connecting to 2g network"));
	#endif

    sim_module->disableGprs(); //forzamos la desconexión de la red de datos por si lo estuviera (esto solo pasaría si por un motivo se resetea/reinicia/apaga arduino pero no el módulo sim) (esto se podría mejorar haciendo un chequeo de "AT+SAPBR=2,1" y que nos devuelva la ip que nos ha asignado, en caso negativo devuelve 0.0.0.0)
    prevention_delay();

    if( !sim_module->enableGprs(gprs_apn,gprs_user,gprs_pass)) //si no consigue conectar
    {
		#if DEBUGGING == true
			Serial.println(S_F(" [-]Fail"));
		#endif
		delay(8500);

      //reintentamos recursivamente antes de reiniciar:
  		if(intento>3)
  			return_status=false;//resetSoftware();
  		else
  		{
  			delay(8500);
  			connect_gprs(intento+1);	//recursivo
  		}
    } 
  	else
  	{
  		return_status=true;
		prevention_delay();
  		#if DEBUGGING == true
			Serial.println(S_F("[+]GPRS connected"));
		#endif
  	}
	  return return_status;
}

void ModuleManager::unlock_sim(void)
{
  
    if ( this->sim_pin[0] != 0 )	//si se definió pin de la tarjeta sim, se desbloquea
    {
      sim_module->getSimState(parameters_buffer, PARAMETERS_SIZE);
      
      if( strcmp(parameters_buffer,"SIM PIN")==0 )
      {
		#if DEBUGGING == true
			Serial.println(S_F("[*]unlocking sim pin"));
		#endif
		sim_module->simUnlock(this->sim_pin);
		prevention_delay();
		prevention_delay();
      }
	}	
}

int8_t ModuleManager::get_sensor_val(uint8_t sensor_pin){	return analogRead(sensor_pin);	}

void ModuleManager::get_time(void/*char *clock_output*/) //tomamos la hora del reloj del módulo sim (cada vez que se conecta a la red se sincroniza automáticamente)
{
	//formato: "20/09/20,04:23:58+08" el 08 es el timezone en cuartos de hora, así que hay que dividirlo entre 4 para que sea el equivalente a 1 hora, así obtendríamos nuestro +2 oficial (pero hay que recordar que la hora ya está con el +2 incluído, así que tal vez debiéramos restarle el +2 a la hora para sumarselo en el servidor al huso horario configurado en él)
	sim_module->getInternalClock(this->current_time);	//<- anteriormente lo devolvíamos al main, ahora lo gestionamos desde la clase
  #if DEBUGGING == true
    Serial.println(F("[*]Getting clock...:")); Serial.println(this->current_time);
  #endif
  prevention_delay();
}

uint8_t ModuleManager::battery_left(void)	
{   // <!!!> se la daremos al HTTP POST como estado si así lo hemos indicado / o lo usaremos para ponerlo en modo bajo consumo
	return sim_module->getBattStat();
}

uint8_t ModuleManager::get_satellites_found(void)
{
	return this->sat;
}

bool ModuleManager::got_gps_position(void)
{  return this->gps_position_found;  }

void ModuleManager::get_gps(void)
{
	float f_lat,f_lon;
	int satellites;
		
	//sim_module->powerOnOffGps(true); //lo encendemos si lo habíamos apagado

	this->gps_position_found=false;
	for(uint8_t i=0; i<3 && !this->gps_position_found; i++)//LOOP de 3 intentos con delays de 60/3 segundos (1 minuto aprox. para encontrar gps's si ha perdido todos los satélites)
	{
		this->gps_status = sim_module->getGpsStatus(parameters_buffer, PARAMETERS_SIZE);


		if(this->gps_status < SIM8xxGpsStatus::Fix) 
		{
			#if DEBUGGING == true
        Serial.println(S_F("[-]GPS: No fix yet..."));
      #endif
			//return false;
			delay(20000);
		}
		else
		{
			this->gps_position_found=true;
			
			sim_module->getGpsField(parameters_buffer, SIM8xxGpsField::GnssUsed, &satellites );
			sim_module->getGpsField(parameters_buffer, SIM8xxGpsField::Latitude,  &f_lat);
			sim_module->getGpsField(parameters_buffer, SIM8xxGpsField::Longitude, &f_lon);

			dtostrf( f_lat, 9, 6, this->lat );
			dtostrf( f_lon, 9, 6, this->lon );
			itoa(satellites, this->sat, 10);
		}
	}
  prevention_delay();
	//sim_module->disableGPS(); // <!!!> deshabilitar, en algún momento ?
}

void ModuleManager::insert_json_parameter(char *var_name,char *var_value,char *buffer)
{	//ejemplo:  "{\"name\": \"morpheus\", \"job\": \"leader\"}";
	strcat(buffer,"\"");
	strcat(buffer,var_name);
	strcat(buffer,"\":\"");
	strcat(buffer,var_value);
	strcat(buffer,"\",");
}

void ModuleManager::generate_json_parameters(char *buffer, char sensor_name[], char time[], char sensor_val[])
{	//Concatenamos los parámetros
	strcpy(buffer,"{");
	insert_json_parameter("station_id",this->station_id, buffer);
	insert_json_parameter("sensor_name",sensor_name, buffer);
	insert_json_parameter("time",time, buffer);
	insert_json_parameter("sensor_val",sensor_val, buffer);
	buffer[strlen(buffer)-1]=0x7D;//sustituímos la última , por } de cierre , solo funciona en hexadecimal, no sabría decir por qué
}

void ModuleManager::generate_aditional_json_parameters(char *buffer, char time[], char status_name[], char status_val[])
{	//Concatenamos los parámetros
	strcpy(buffer,"{");
	insert_json_parameter("station_id",this->station_id, buffer);
	insert_json_parameter("time",time, buffer);
	
	if( strcmp(status_name,"lat")==0 || strcmp(status_name,"lon")==0 )
	{
		if( this->use_gps && this->gps_position_found )   //solo manda la ubicación si la encuentra
		{
			if( strcmp(status_name,"lat")==0 )
			{
				insert_json_parameter("status_name","lat", buffer);
				insert_json_parameter("status_val",this->lat, buffer);
			}
			else
			{
				insert_json_parameter("status_name","lon", buffer);
				insert_json_parameter("status_val",this->lon, buffer);
			}
		}
	}
	else
	{
		insert_json_parameter("status_name",status_name, buffer);
		insert_json_parameter("status_val",status_val, buffer);
	}
	buffer[strlen(buffer)-1]=0x7D;//sustituímos la última , por } de cierre (solo funciona en hexadecimal?)
}

bool ModuleManager::send_data_to_server(char sensor_name[], char sensor_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	generate_json_parameters(parameters_buffer,sensor_name,this->current_time,sensor_val);
	return send_http_post(parameters_buffer);
}

bool ModuleManager::send_aditional_data_to_server(char status_name[],char status_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	generate_aditional_json_parameters(parameters_buffer, this->current_time, status_name, status_val);
	return send_http_post(parameters_buffer);
}

bool ModuleManager::send_http_post(char *parameters_buffer)
{
	uint16_t responseCode;

	#if DEBUGGING == true
		Serial.println("");Serial.print(F("[+]HTTP PARAMETERS: ")); Serial.print(F("\t")); Serial.print(parameters_buffer);
	#endif
	
	responseCode = sim_module->httpPost(this->server_address, S_F("application/json"), parameters_buffer, parameters_buffer, BUFFER_SIZE);

	#if DEBUGGING == true
		Serial.println("");
		Serial.print(F("[+]Server response : "));Serial.print(parameters_buffer);Serial.println("");
		Serial.println(F("[+]Server response code: "));Serial.print(responseCode);Serial.println("");Serial.println("");
	#endif
    
	return (responseCode==200);
}

void ModuleManager::write_sd(char parameters_buffer[])
{
	myFile = SD.open(this->file_name, FILE_WRITE);
	if (myFile) 
	{
		#if DEBUGGING == true
			Serial.println(F("[+]{save_data_in_sd} writing to file..."));		
		#endif
		myFile.println(parameters_buffer);
		myFile.close();

		#if DEBUGGING == true
			Serial.println(F("[+]{save_data_in_sd} done writting."));
		#endif
	}
	else 
	{
		#if DEBUGGING == true
			Serial.println(F("[-]{save_data_in_sd} error opening file for WRITE"));
		#endif
	}	
}

void ModuleManager::save_data_in_sd(char sensor_name[], char sensor_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	generate_json_parameters(parameters_buffer, sensor_name, this->current_time, sensor_val);
	this->write_sd(parameters_buffer);
}

void ModuleManager::save_aditional_data_in_sd(char status_name[],char status_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	generate_aditional_json_parameters(parameters_buffer, this->current_time, status_name, status_val);
	this->write_sd(parameters_buffer);
}


bool ModuleManager::data_waiting_in_sd()
{
	bool is_data_waiting=false;
	
	this->myFile = SD.open(this->file_name);
	if (this->myFile) 
	{
		if(this->myFile.available())
			is_data_waiting=true;

		myFile.close();
	}
	return is_data_waiting;
}

bool ModuleManager::send_last_sd_line()
{
	char line_buffer[PARAMETERS_SIZE];
	
	this->myFile = SD.open(this->file_name);
	if (this->myFile) 
	{
		while (this->myFile.available()) 
		{
			#if DEBUGGING == true
				Serial.println(F("[*]reading line..."));
			#endif

			for(uint8_t i=0;i<PARAMETERS_SIZE;i++) // continúa mientras el buffer tenga espacio (no podemos almacenar en memoria todas las líneas para enviarlas, así que envíamos cada una en cuanto la lee)
			{
				line_buffer[i]=this->myFile.read();
				if (line_buffer[i] == '\n' || !line_buffer[i] ) //ha llegado al fin de línea o del fichero
				{
					line_buffer[i]='\0';  //marcamos el final del buffer
					#if DEBUGGING == true
						Serial.println(line_buffer);
					#endif

					if(line_buffer[0]=="{" && line_buffer[i-2] == "}")  //verificamos que esté correctamente formateada la línea (i-2 del \r\n)
					{
						if ( !send_http_post(line_buffer) )
						{
							#if DEBUGGING == true
								Serial.println(F("[-]error sending SD data to server"));
							#endif
							myFile.close();
							return false;
						}
					}
					if(line_buffer[i]=='\0')
						break;
				}
			}
		}
		myFile.close();
	}
	SD.remove(this->file_name);  
	return true;
}

void ModuleManager::prevention_delay(){delay(2000);}