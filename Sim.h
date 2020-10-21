#define SRVADD_SIZE 50
#define GPSBUFFER_SIZE 128
#define PARAMETERS_SIZE 200

Class Sim
{
	private:
	SIM8xx sim_module;
	char parameters_buffer[PARAMETERS_SIZE];
	const char server_address[SRVADD_SIZE];
	
	SIM8xxGpsStatus gps_status;
	char gps_position_buffer[GPSBUFFER_SIZE];
	float lat, lon;
	
	boolean connection_successful;
	boolean debug;
	boolean gps;

	public:
		Sim(byte , byte);
		(* resetSoftware)(void);
		void connect_network(int intento=1);
		void connect_gprs(int intento=1);
		String get_time(void);
		int8_t battery_left(void);
		void get_time(char *);
		

}