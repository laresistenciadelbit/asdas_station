Sim::Sim(const byte sim_tx, const byte sim_rx)
{

	Serial.begin(SERIAL_BAUDRATE);		//podemos meterlo como cte en la clase el parametro
	Log.begin(LOG_LEVEL_NOTICE, &Serial);	//para debug

	simSerial = SoftwareSerial(sim_tx, sim_rx);

	simSerial.begin(SIM_BAUDRATE);


#if defied(USE_SIM808)
	sim = SIM808(SIM_RST, SIM_PWR);
	sim.begin(simSerial);
	Log.notice(S_F("Powering on SIM808..." NL));

	sim808.powerOnOff(true);

	sim808.init();
	Log.notice(S_F("Powering on SIM808's GPS..." NL));
	sim808.powerOnOffGps(true);
#else
	sim = SIM800L( (Stream *)simSerial, SIM_RST, 200, 512, (Stream *)&Serial );
#endif
	
	
	
	
}