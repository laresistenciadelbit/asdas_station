#include <SPI.h>
#include "SdFat.h"
#include <SIM8xx.h>

#include "ModuleManager.h"

ModuleManager::ModuleManager(const char *id, const char *server, const bool gps, const bool debug, const int8_t pwkey, const int8_t rst ,char *pin, char *apn, char *user, char *pass)
{
	strlcpy(this->station_id,id,sizeof(id));
	strlcpy(this->server_address,server,sizeof(server));

	strcpy(this->gprs_apn,apn);
	if(user!=NULL)
	{
		strcpy(this->gprs_user,user);
		strcpy(this->gprs_pass,pass);
	}
  
	if(pin[0]!=NULL)
	{
		strlcpy(this->sim_pin,pin,4);
	}
	else
		this->sim_pin[0]=NULL;

	if (debug)
		this->debug_mode=true;
	else
	  this->debug_mode=false;
      
// Inicializamos módulos

	// Inicializamos módulo micro SD
	if (!SD.begin(SS))
		if(this->debug_mode)
			Serial.println(F("[-]SD initialization failed!"));
	else
		if(this->debug_mode)
			Serial.println(F("[+]SD initialization done."));

	//Inicializamos comunicación con el pc solo para debugging
	if (this->debug_mode)
	{
		Serial.begin(115200); //iniciamos el puerto de comunicación del microcontrolador (recomendado >9600 y <115200)
//		Log.begin(LOG_LEVEL_NOTICE, &Serial);
		delay(10);
	}
	
  //Inicializamos comunicación en serie desde microcontrolador a módulo sim
	Serial1.begin(9600);	//iniciamos la vía de comunicación con el módulo sim
	
	this->sim_module = new SIM8xx(pwkey,rst);//SIM_RST, SIM_PWR);   // <- arduino no permite instanciar clases, por lo que creamos la clase SIM8xx y la inicializamos directamente desde la definicion de esta clase (en ModuleManager.h)
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

	if(this->debug_mode)
		Serial.print(S_F("[*]Waiting for network..."));
	
	network_status = sim_module->getNetworkRegistrationStatus();
	if( static_cast<int8_t>(network_status) & (static_cast<int8_t>(SIM8xxNetworkRegistrationState::Registered) | static_cast<int8_t>(SIM8xxNetworkRegistrationState::Roaming)) == 0 )
	{
		Serial.println(S_F(" [-]Fail"));
		
		if(intento>3)
			return_status=false;//resetSoftware();
		else
		{
			delay(8500);
			return_status=connect_network(intento+1);	//recursivo
		}
	}
	else
	{
		if(this->debug_mode)
			Serial.println("[+]Network connected");
		return_status=true;
	}
	return return_status;
}

bool ModuleManager::connect_gprs(uint8_t intento=0)
{
	bool return_status;
	
	this->unlock_sim(); //solo desbloquea en caso de que no se haya desbloqueado (que ya debería estarlo)
	
	// GPRS connection parameters are usually set after network registration
	if(this->debug_mode)Serial.print(S_F("[*]Connecting to 2g network"));
    if (!sim_module->enableGprs(gprs_apn,gprs_user,gprs_pass))
    {
      if(debug_mode)Serial.println(S_F(" [-]Fail"));
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
		if(this->debug_mode)Serial.println("[+]GPRS connected");
	}
	return return_status;
}

void ModuleManager::unlock_sim(void)
{
  
    if ( this->sim_pin[0] != NULL )	//si se definió pin de la tarjeta sim, se desbloquea
    {
      sim_module->getSimState(parameters_buffer, PARAMETERS_SIZE);
      
      if( strcmp(parameters_buffer,"SIM PIN")==0 )
      {
		if(this->debug_mode) 
			Serial.println(F("[*]unlocking sim pin"));
		sim_module->simUnlock(this->sim_pin);
		delay(6000);
      }
	}	
}

int8_t ModuleManager::get_sensor_val(uint8_t sensor_pin){	return analogRead(sensor_pin);	}

void ModuleManager::get_time(void/*char *clock_output*/) //tomamos la hora del reloj del módulo sim (cada vez que se conecta a la red se sincroniza automáticamente)
{
	//formato: "20/09/20,04:23:58+08" el 08 es el timezone en cuartos de hora, así que hay que dividirlo entre 4 para que sea el equivalente a 1 hora, así obtendríamos nuestro +2 oficial (pero hay que recordar que la hora ya está con el +2 incluído, así que tal vez debiéramos restarle el +2 a la hora para sumarselo en el servidor al huso horario configurado en él)
	sim_module->getInternalClock(this->current_time);	//<- anteriormente lo devolvíamos al main, ahora lo gestionamos desde la clase
}

uint8_t ModuleManager::battery_left(void)	
{ 
  // <!!!> se la daremos al GET/POST si así lo hemos indicado en una constante / o lo usaremos para ponerlo en modo bajo consumo
	return sim_module->getBattStat();
}

uint8_t ModuleManager::get_satellites_found(void)
{
	return this->sat;
}

void ModuleManager::get_gps(void)
{
	char lat_lon_buffer[12];
	float f_lat,f_lon;
	uint8_t satellites;
		
	sim_module->powerOnOffGps(true); //lo encendemos si lo habíamos apagado

	this->gps_position_found=false;
	for(int i=0; i<3 && !this->gps_position_found; i++)//LOOP de 3 intentos con delays de 60/3 segundos (1 minuto aprox. para encontrar gps's si ha perdido todos los satélites)
	{
		this->gps_status = sim_module->getGpsStatus(gps_position_buffer, 128);


		if(this->gps_status < SIM8xxGpsStatus::Fix) 
		{
			if(this->debug_mode)	
			  Serial.println(S_F("[-]GPS: No fix yet..."));
			//return false;
			delay(60000/3);
		}
		else
		{
			this->gps_position_found=true;
			
			sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::GnssUsed, &satellites );
			sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::Latitude,  &f_lat);
			sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::Longitude, &f_lon);

			dtostrf( f_lat, 9, 6, this->lat );
			dtostrf( f_lon, 9, 6, this->lon );
			this->sat=satellites;//itoa(sattelites, this->sat, 10);
		}
	}
	//sim_module->disableGPS(); // <!!!> deshabilitar, en algún momento ?
}

void ModuleManager::insert_json_parameter(char *var_name,char *var_value,char *buffer)
{
	//ejemplo:  "{\"name\": \"morpheus\", \"job\": \"leader\"}";
	strlcpy(buffer,"\"",PARAMETERS_SIZE);
	strlcpy(buffer,var_name,PARAMETERS_SIZE);
	strlcpy(buffer,"\": \"",PARAMETERS_SIZE);
	strlcpy(buffer,var_value,PARAMETERS_SIZE);
	strlcpy(buffer,"\", ",PARAMETERS_SIZE);
}

void ModuleManager::generate_json_parameters(char *buffer, char sensor_name[], char time[], char sensor_val[])
{
	//Concatenamos los parámetros
	strlcpy(buffer,"{",PARAMETERS_SIZE);
	insert_json_parameter("station_id",this->station_id, buffer);
	insert_json_parameter("sensor_name",sensor_name, buffer);
	insert_json_parameter("time",time, buffer);
	insert_json_parameter("sensor_val",sensor_val, buffer);
/*
	if(this->use_gps && gps_position_found)
	{
		insert_json_parameter("lat",this->lat, buffer);
		insert_json_parameter("lon",this->lon, buffer);
	}
*/
	strlcpy(parameters_buffer,"}",PARAMETERS_SIZE);
}

void ModuleManager::generate_aditional_json_parameters(char *buffer, char time[], char status_name[], char status_val[])
{
	//Concatenamos los parámetros
	strlcpy(buffer,"{",PARAMETERS_SIZE);
	insert_json_parameter("station_id",station_id, buffer);
	insert_json_parameter("time",time, buffer);
	
	if( ( strcmp(status_name,"lat")==0 || strcmp(status_name,"lon")==0 ) && this->use_gps && gps_position_found)
	{
		insert_json_parameter("lat",this->lat, buffer);
		insert_json_parameter("lon",this->lon, buffer);
	}
	else
	{
		insert_json_parameter(status_name,status_val, buffer);
	}
	
	strlcpy(parameters_buffer,"}",PARAMETERS_SIZE);
}

bool ModuleManager::send_data_to_server(char sensor_name[], char sensor_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	generate_json_parameters(parameters_buffer,this->station_id,sensor_name,sensor_val,this->current_time);
	
	return send_http_post(parameters_buffer);
}

bool ModuleManager::send_aditional_data_to_server(char status_name[],char status_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	generate_aditional_json_parameters(parameters_buffer, this->station_id, this->current_time, status_name, status_val);
	
	return send_http_post(parameters_buffer);
}

bool ModuleManager::send_http_post(char *parameters_buffer)
{
  uint16_t responseCode;
	responseCode = sim_module->httpPost(this->server_address, S_F("application/json"), parameters_buffer, parameters_buffer, BUFFER_SIZE);

	return (responseCode==200);	// !!! <-- en todo caso devolvería 200 en string?
}

void ModuleManager::save_data_in_sd(char sensor_name[], char sensor_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	myFile = SD.open(this->file_name, FILE_WRITE);
	if (myFile) 
	{
		if(this->debug_mode) 
			Serial.println(F("[+]{save_data_in_sd} writing to test.txt..."));
		
		generate_json_parameters(parameters_buffer, this->station_id, sensor_name, sensor_val, this->current_time);
		
		//myFile.println("1,2020/09/28_20:25+2,0.38651,-0.571498,101.77");
		//myFile.println("1,2020/09/28_20:38+2,0.38637,-0.571411,134.22");
		myFile.println(parameters_buffer);
		myFile.close();

		if(this->debug_mode) 
			Serial.println(F("[+]{save_data_in_sd} done writting."));
	} 
	else 
	{
		if(this->debug_mode) 
			Serial.println(F("[-]{save_data_in_sd} error opening file for WRITE"));
	}
}

void ModuleManager::save_aditional_data_in_sd(char status_name[],char status_val[])
{
	char parameters_buffer[PARAMETERS_SIZE];
	
	myFile = SD.open(this->file_name, FILE_WRITE);
	if (myFile) 
	{
		if(this->debug_mode) 
			Serial.println(F("[+]{save_aditional_data_in_sd} writing to test.txt..."));
		
		generate_aditional_json_parameters(parameters_buffer, this->station_id, this->current_time, status_name, status_val);
		
		myFile.println(parameters_buffer);
		myFile.close();

		if(this->debug_mode)
			Serial.println(F("[+]{save_aditional_data_in_sd} done writting."));
	}
	else
	{
		if(this->debug_mode) 
			Serial.println(F("[-]{save_aditional_data_in_sd} error opening file for WRITE"));
	}
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
		  if(this->debug_mode) 
		    Serial.println(F("[*]reading line..."));
       
		  for(int i=0;i<PARAMETERS_SIZE;i++)
		  {
			line_buffer[i]=this->myFile.read();
			if (line_buffer[i] == '\n') //ha llegado al fin de línea
			{
				line_buffer[i]='\0'; //nueva línea; fin de buffer
				if(this->debug_mode)
					Serial.println(line_buffer);
				//no podemos almacenar en memoria todas las líneas para enviarlas, así que envíamos cada una en cuanto la lee
				if ( !send_http_post(line_buffer) )
				{
					if(this->debug_mode)
						Serial.println(F("[-]error sending SD data to server"));
					myFile.close();
					return false;
				}
			}
		  }
		  
		  
		}
		myFile.close();
	}
	SD.remove(this->file_name);  
	return true;
}
