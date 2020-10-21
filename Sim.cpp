Sim::Sim(const *server, const boolean gps, const boolean debug)
{
	STRLCPY_P(this->server_address,server);
	
	if (gps)
	{
		this->use_gps=true;
		sim_module.powerOnOffGps(true);
	}
	else
		this->use_gps=false;
		
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
	
	this->sim_module = SIM8xx(SIM_RST, SIM_PWR);
	this->sim_module.begin(Serial1);
	this->sim_module.powerOnOff(true);
    this->sim_module.init(); 
	

	if( connect_network() )
	{
		this->connection_successful=connect_gprs();
	}
	else
		this->connection_successful=false;
	
	
}

//void Sim::(* resetSoftware)(void) = 0; //software reset //then call with resetSoftware();

boolean Sim::is_full_connected(){return this->connection_successful;}

boolean Sim::connect_network(int intento=0)
{
	SIM808NetworkRegistrationState network_status;
	boolean return_status;
	
	this->unlock_sim();

	if(this->debug_mode)Serial.print("[+]Waiting for network...");
	
	network_status = sim_module.getNetworkRegistrationStatus();
	if( static_cast<int8_t>(network_status) & (static_cast<int8_t>(SIM808NetworkRegistrationState::Registered) | static_cast<int8_t>(SIM808NetworkRegistrationState::Roaming)) == 0 )
	{
		Serial.println(" [-]Fail");
		
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

void Sim::connect_gprs(int intento=0)
{
	boolean return_status;
	
	this->unlock_sim(); //solo desbloquea en caso de que no se haya desbloqueado (que ya debería estarlo)
	
	// GPRS connection parameters are usually set after network registration
	if(this->debug_mode)Serial.print("[+]Connecting to 2g network");
    if (!sim_module.enableGprs(GPRS_APN,GPRS_USER,GPRS_PASS)) //, gprsUser, gprsPass)) 
    {
      if(debug_mode)Serial.println(" [-]Fail");
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

void Sim::unlock_sim()
{
    if ( GSM_PIN && sim_module.getSimStatus() != 3 )	//si se definió pin de la tarjeta sim, se desbloquea
    {
		sim_module.simUnlock(GSM_PIN);
		delay(6000);
	}	
}

void Sim::get_time(char *clock_output) //tomamos la hora del reloj del módulo sim (cada vez que se conecta a la red se sincroniza automáticamente)
{
	//formato: "20/09/20,04:23:58+08" el 08 es el timezone en cuartos de hora, así que hay que dividirlo entre 4 para que sea el equivalente a 1 hora, así obtendríamos nuestro +2 oficial (pero hay que recordar que la hora ya está con el +2 incluído, así que tal vez debiéramos restarle el +2 a la hora para sumarselo en el servidor al huso horario configurado en él)
	sim_module.getInternalClock(clock_output);
}

void Sim::battery_left()	// <!!!> se la daremos al GET/POST si así lo hemos indicado en una constante
{
	uint8_t  chargeState = -99;
	int8_t   percent     = -99;
	uint16_t milliVolts  = -9999;
	sim_module.getBattStats(chargeState, percent, milliVolts);
	return percent;
}


boolean Sim::getGPS()	// <!!!> MODIFICAR para tomar &lat2 &lon2 fuera de la función (variables de la clase)(el resto variables locales como están ahora)
{
	sim_module.powerOnOffGps(true); //lo encendemos si lo habíamos apagado
	
	gps_status = sim_module.getGpsStatus(gps_position_buffer, 128);


	if(status < SIM808GpsStatus::Fix) {
		if(this->debug_mode)	Log.notice(S_F("No fix yet..." NL));
//		delay(NO_FIX_GPS_DELAY);	// OJO CON ESTE DELAY !!!!!!!!!
		return false;
	}

	//sim_module.getGpsField(gps_position_buffer, SIM808GpsField::GnssUsed, &sattelites);
	sim_module.getGpsField(gps_position_buffer, SIM8xxGpsField::Latitude,  &lat);
    sim_module.getGpsField(gps_position_buffer, SIM8xxGpsField::Longitude, &lon);

	return true;
	
	//sim_module.disableGPS(); // <!!!> deshabilitar, en algún momento ?
}

void Sim::insert_json_parameter(char *var_name,char *var_value)
{
	//ejemplo:  "{\"name\": \"morpheus\", \"job\": \"leader\"}";
	strlcpy(parameters_buffer,"\"",PARAMETERS_SIZE);
	strlcpy(parameters_buffer,var_name,PARAMETERS_SIZE);
	strlcpy(parameters_buffer,"\": \"",PARAMETERS_SIZE);
	strlcpy(parameters_buffer,var_value,PARAMETERS_SIZE);
	strlcpy(parameters_buffer,"\", ",PARAMETERS_SIZE);
}

boolean Sim::send_http_post(char url[], char id[], char sensor_number[], char sensor_value[], char time[])
{
	char parameters_buffer[200];
	uint16_t responseCode;
	
	//Concatenamos los parámetros de la petición POST
	strlcpy(parameters_buffer,"{",PARAMETERS_SIZE);
	insert_json_parameter(S_F("id"),id);
	insert_json_parameter(S_F("sensor_id"),sensor_value);
	insert_json_parameter(S_F("time"),get_time());
	
	if(this->use_gps)
	{
		insert_json_parameter(S_F("lat"),this->lat);
		insert_json_parameter(S_F("lon"),this->lon);
	}
	strlcpy(parameters_buffer,"}",PARAMETERS_SIZE);
	
	//Realizamos la peticion http post
	responseCode = sim8xx.httpPost(this->server_address, S_F("application/json"), parameters_buffer, parameters_buffer, BUFFER_SIZE);

	return (responseCode==200);
}
