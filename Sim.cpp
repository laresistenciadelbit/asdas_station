Sim::Sim(boolean debug_mode)
{
	SoftwareSerial SerialAT(PIN_RX, PIN_TX);
	
	modem(SerialAT);
	client(modem);
	http(client, server, port);
	
	digitalWrite(PIN_PWKEY, LOW);
	digitalWrite(PIN_RST, HIGH);

	SerialAT.begin(9600);	//iniciamos la vía de comunicación con el módulo sim
	delay(6000);
	modem.restart();			//reiniciamos el módulo sim para sincronizar la comunicación
	delay(2000);
	
	if (debug_mode)
		this->debug=true;
	else
		this->debug=false;
	
	this->connect_network();
	this->connect_gprs();
	
}

void Sim::(* resetSoftware)(void) = 0; //software reset
	//then call with resetSoftware();
	
void Sim::connect_network(int intento)
{
	this->unlock_sim();

	
	if(debug)Serial.print("[+]Waiting for network...");
	
	if (!modem.waitForNetwork()) 
	{
		Serial.println(" [-]Fail");
		
		if(intento>3)
			resetSoftware();//return;
		else
		{
			delay(8000);
			this->connect_network(intento+1);	//recursivo
		}
	}
	if(debug)Serial.println("[+]Network connected");

	
}

void Sim::connect_gprs(int intento)
{
	this->unlock_sim(); //solo desbloquea en caso de que no se haya desbloqueado (que ya debería estarlo)
	
	// GPRS connection parameters are usually set after network registration
	if(debug)Serial.print("[+]Connecting to 2g network");
    if (!modem.gprsConnect(apn)) //, gprsUser, gprsPass)) 
    {
      if(debug)Serial.println(" [-]Fail");
      delay(8000);

      //reintentamos recursivamente antes de reiniciar:
		if(intento>3)
			resetSoftware();//return;
		else
		{
			delay(8000);
			this->connect_gprs(intento+1);	//recursivo
		}
    }

    if (modem.isGprsConnected()) {
      if(debug)Serial.println("GPRS connected");
    }
	
}

void Sim::unlock_sim()
{
    if ( GSM_PIN && modem.getSimStatus() != 3 )	//si se definió pin de la tarjeta sim, se desbloquea
    {
		modem.simUnlock(GSM_PIN);
		delay(6000);
	}	
}

void Sim::get_time() //tomamos la hora del reloj del módulo sim (cada vez que se conecta a la red se sincroniza automáticamente)
{
	//formato: "20/09/20,04:23:58+08" el 08 es el timezone en cuartos de hora, así que hay que dividirlo entre 4 para que sea el equivalente a 1 hora, así obtendríamos nuestro +2 oficial (pero hay que recordar que la hora ya está con el +2 incluído, así que tal vez debiéramos restarle el +2 a la hora para sumarselo en el servidor al huso horario configurado en él)
	return modem.getGSMDateTime(DATE_FULL);
}

void Sim::battery_left()	// <!!!> se la daremos al GET/POST si así lo hemos indicado en una constante
{
	uint8_t  chargeState = -99;
	int8_t   percent     = -99;
	uint16_t milliVolts  = -9999;
	modem.getBattStats(chargeState, percent, milliVolts);
	return percent;
}


void Sim::getGPS()	// <!!!> MODIFICAR para tomar &lat2 &lon2 fuera de la función (variables de la clase)(el resto variables locales como están ahora)
{
	modem.enableGPS();
	
	// <!!!> OJO CON EL ENABLE; no creo q resetee el gps al volverlo a llamar, pero por si a caso tener en cuenta y mirarlo.
		
	// <!!!> CREAR DELAY HASTA QUE ENCUENTRE SATÉLITES (mínimo 1 minuto?)
	
	float lat2      = 0;
	float lon2      = 0;
	float speed2    = 0;
	float alt2      = 0;
	int   vsat2     = 0;
	int   usat2     = 0;
	float accuracy2 = 0;
	int   year2     = 0;
	int   month2    = 0;
	int   day2      = 0;
	int   hour2     = 0;
	int   min2      = 0;
	int   sec2      = 0;
  
  
	if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2, &year2, &month2, &day2, &hour2, &min2, &sec2)) 
	{
		if(usat2>=3) // ???? <- lo he puesto para tomar el valor del gps únicamente si está tomando las coordenadas
		{			 // presuponiendo que el valor mínimo de satelites usados han de ser 3 para tomar las coordenadas correctamente
					 // pero lo más seguro es que podamos saber si ha obtenido bien las coordenadas con otra variable como "accuracy2" o viendo si "lat2" no es NULL
			DBG("Latitude:", String(lat2, 8), "\tLongitude:", String(lon2, 8));
			DBG("Speed:", speed2, "\tAltitude:", alt2);
			DBG("Visible Satellites:", vsat2, "\tUsed Satellites:", usat2);
			DBG("Accuracy:", accuracy2);
			DBG("Year:", year2, "\tMonth:", month2, "\tDay:", day2);
			DBG("Hour:", hour2, "\tMinute:", min2, "\tSecond:", sec2);
		}
	}
	
	// otra forma es tomarlo en bruto directamente: String gps_raw = modem.getGPSraw();
	
	
	//modem.disableGPS(); // <!!!> deshabilitar, en algún momento ?
}


void Sim::send_http()
{
	#if defined(USE_HTTP_POST)
		this->HTTPS_POST();
	#else
		this->HTTPS_GET();
	#endif
	
}

void Sim::HTTPS_POST()
{
	
}

void Sim::HTTPS_GET()
{
	
  if(debug)Serial.print(F("Performing HTTPS GET request... "));
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  int err = http.get(resource);
  if (err != 0) {
    if(debug)Serial.println(F("failed to connect"));
    delay(8000);
    return;
  }

  int status = http.responseStatusCode();
  if(debug)Serial.print(F("Response status code: "));
  if(debug)Serial.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  if(debug)Serial.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    if(debug)Serial.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    if(debug)Serial.print(F("Content length is: "));
    if(debug)Serial.println(length);
  }
  if (http.isResponseChunked()) {
    if(debug)Serial.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  if(debug)Serial.println(F("Response:"));
  if(debug)Serial.println(body);

  if(debug)Serial.print(F("Body length is: "));
  if(debug)Serial.println(body.length());

  // Shutdown

  http.stop();
  if(debug)Serial.println(F("Server disconnected"));
	
	
	
	
	
	
	
}