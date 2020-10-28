#include <SPI.h>
#include "SdFat.h"
#include <SIM8xx.h>

//#define PIN_PWKEY 7
//#define PIN_RST 6

#define SRVADD_SIZE 50
#define GPSBUFFER_SIZE 128
#define PARAMETERS_SIZE 200

class ModuleManager
{
	private:
	SIM8xx *sim_module;
	char sim_pin[4];
	char gprs_apn[24];
	char gprs_user[32];
	char gprs_pass[16];
	char parameters_buffer[PARAMETERS_SIZE];
	char server_address[SRVADD_SIZE];

	SdFat SD;
	File myFile;
	const char *file_name={"data.txt"};
  
	SIM8xxGpsStatus gps_status;
	char gps_position_buffer[GPSBUFFER_SIZE];
	char lat[10], lon[10];
	
	bool connection_successful;
	bool debug_mode;
	bool use_gps;

	public:
		ModuleManager(const char *server, const bool gps, const bool debug, const int8_t pwkey, const int8_t rst ,char *pin, char *apn, char *user, char *pass);
//		(* resetSoftware)(void);
		bool is_full_connected(void);
		bool connect_network(uint8_t);
		bool connect_gprs(uint8_t);
		void unlock_sim(void);
		int8_t battery_left(void);
    
		bool get_gps(void);
		void get_time(char *);
		int8_t get_sensor_value(uint8_t);
   
		void insert_json_parameter(char *, char *, char *);
		void generate_json_parameters(char *, char *, char *, char *, char *);
    bool send_data_to_server(char *, char *, char *, char *);
		bool send_http_post(char *);

    void save_in_sd_card(char *, char *, char *, char *);
    bool data_waiting_in_sd(void);
    bool send_last_sd_line(void);
};
