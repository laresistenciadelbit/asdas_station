Class Sim
{
	private:
	TinyGsm modem;
	TinyGsmClientSecure client;
	HttpClient http;
	
	SoftwareSerial SerialAT;

	boolean debug;

#if defied(USE_SIM808)
		SIM808 sim;
#else
		SIM800L sim;
#endif


	public:
		Sim(byte , byte);
		(* resetSoftware)(void);
		void connect_network(int intento=1);
		void connect_gprs(int intento=1);
		String get_time(void);
		int8_t battery_left(void);
		

}