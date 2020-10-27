#include <SIM8xx.h>
#include "ModuleManager.h"

ModuleManager::ModuleManager(const char *server, const bool gps, const bool debug, const int8_t pwkey, const int8_t rst ,char *pin, char *apn, char *user, char *pass)
{
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
	{
		this->debug_mode=true;
		Serial.begin(115200); //iniciamos el puerto de comunicación del microcontrolador (recomendado >9600 y <115200)
		Log.begin(LOG_LEVEL_NOTICE, &Serial);
	delay(10);
	}
	else
		this->debug_mode=false;
		
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

//void ModuleManager::(* resetSoftware)(void) = 0; //software reset //then call with resetSoftware();

bool ModuleManager::is_full_connected(void){return this->connection_successful;}

bool ModuleManager::connect_network(uint8_t intento=0)
{
	SIM8xxNetworkRegistrationState network_status;
	bool return_status;
	
	this->unlock_sim();

	if(this->debug_mode)Serial.print(S_F("[+]Waiting for network..."));
	
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
		if(this->debug_mode)Serial.println("[+]Network connected");
		return_status=true;
	}
	return return_status;
}

bool ModuleManager::connect_gprs(uint8_t intento=0)
{
	bool return_status;
	
	this->unlock_sim(); //solo desbloquea en caso de que no se haya desbloqueado (que ya debería estarlo)
	
	// GPRS connection parameters are usually set after network registration
	if(this->debug_mode)Serial.print(S_F("[+]Connecting to 2g network"));
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
		if(this->debug_mode)Serial.println("GPRS connected");
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
    		sim_module->simUnlock(this->sim_pin);
    		delay(6000);
      }
	}	
}

int8_t ModuleManager::get_sensor_value(uint8_t sensor_pin){	return analogRead(sensor_pin);	}

void ModuleManager::get_time(char *clock_output) //tomamos la hora del reloj del módulo sim (cada vez que se conecta a la red se sincroniza automáticamente)
{
	//formato: "20/09/20,04:23:58+08" el 08 es el timezone en cuartos de hora, así que hay que dividirlo entre 4 para que sea el equivalente a 1 hora, así obtendríamos nuestro +2 oficial (pero hay que recordar que la hora ya está con el +2 incluído, así que tal vez debiéramos restarle el +2 a la hora para sumarselo en el servidor al huso horario configurado en él)
	sim_module->getInternalClock(clock_output);
}

int8_t ModuleManager::battery_left(void)	
{ 
  // <!!!> se la daremos al GET/POST si así lo hemos indicado en una constante / o lo usaremos para ponerlo en modo bajo consumo
	return sim_module->getBattStat();
}


bool ModuleManager::get_gps(void)
{
	sim_module->powerOnOffGps(true); //lo encendemos si lo habíamos apagado
	
	this->gps_status = sim_module->getGpsStatus(gps_position_buffer, 128);


	if(this->gps_status < SIM8xxGpsStatus::Fix) 
	{
		if(this->debug_mode)	
		  Log.notice(S_F("No fix yet..."));
//	delay(NO_FIX_GPS_DELAY);	// OJO CON ESTE DELAY !!!!!!!!!
		return false;
	}

	//sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::GnssUsed, &sattelites);
  sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::Latitude,  &lat);
  sim_module->getGpsField(gps_position_buffer, SIM8xxGpsField::Longitude, &lon);

	return true;
	
	//sim_module->disableGPS(); // <!!!> deshabilitar, en algún momento ?
}

void ModuleManager::insert_json_parameter(char *var_name,char *var_value)
{
	//ejemplo:  "{\"name\": \"morpheus\", \"job\": \"leader\"}";
	strlcpy(parameters_buffer,"\"",PARAMETERS_SIZE);
	strlcpy(parameters_buffer,var_name,PARAMETERS_SIZE);
	strlcpy(parameters_buffer,"\": \"",PARAMETERS_SIZE);
	strlcpy(parameters_buffer,var_value,PARAMETERS_SIZE);
	strlcpy(parameters_buffer,"\", ",PARAMETERS_SIZE);
}

bool ModuleManager::send_http_post(char id[], char sensor_number[], char sensor_value[], char time[])
{
  char lat_lon_buffer[12];
	char parameters_buffer[200];
	uint16_t responseCode;
	
	//Concatenamos los parámetros de la petición POST
	strlcpy(parameters_buffer,"{",PARAMETERS_SIZE);
	insert_json_parameter("id",id);
	insert_json_parameter("sensor_id",sensor_value);
	insert_json_parameter("time",time);
	
	if(this->use_gps)
	{
    dtostrf( this->lat, 1, 6, lat_lon_buffer );
    insert_json_parameter("lat",lat_lon_buffer);
    dtostrf( this->lon, 1, 6, lat_lon_buffer );
		insert_json_parameter("lon",lat_lon_buffer);
	}
	strlcpy(parameters_buffer,"}",PARAMETERS_SIZE);
	
	//Realizamos la peticion http post
	responseCode = sim_module->httpPost(this->server_address, S_F("application/json"), parameters_buffer, parameters_buffer, BUFFER_SIZE);

	return (responseCode==200);
}
