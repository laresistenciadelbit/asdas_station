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
		char station_id[18];
		//char password[18];
	
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
		bool use_gps;
		bool gps_position_found;
		char gps_position_buffer[GPSBUFFER_SIZE];
		char lat[10];
		char lon[10];
		char sat[3];
		
		char current_time[20]; //almacenamos el tiempo de adquisición de los sensores
		
		bool connection_successful;
		bool debug_mode;
		
	public:
		ModuleManager(const char* passwd, const char *id, const char *server, const bool gps, const bool debug, const int8_t pwkey, const int8_t rst ,char *pin, char *apn, char *user, char *pass);

		bool is_full_connected(void);
		bool connect_network(uint8_t);
		bool connect_gprs(uint8_t);
		void unlock_sim(void);
		uint8_t battery_left(void);
		uint8_t get_satellites_found(void);
		void get_gps(void);
		void get_time(void);
		int8_t get_sensor_val(uint8_t);
   
		void insert_json_parameter(char *, char *, char *);
		void generate_json_parameters(char *, char *, char *, char *);
		void generate_aditional_json_parameters(char *, char *, char *, char *);
		bool send_data_to_server(char *, char *);
		bool send_aditional_data_to_server(char *,char *);
		bool send_http_post(char *);

		void save_data_in_sd(char *, char *);
		void save_aditional_data_in_sd(char *,char *);
		bool data_waiting_in_sd(void);
		bool send_last_sd_line(void);
};
