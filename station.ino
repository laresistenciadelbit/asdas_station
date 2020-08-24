  #define USE_SIM808  1 //si está activo usa la librería de SIM808 con soporte a gps
//#define USE_SIM800L 1 //si está activa usa la librería de SIM800L

#define USE_PIN 1 //si tenemos la tarjeta sim con pin configurado

#include <Time.h>     //para usar arduino como un reloj real tomando los datos de las células móvil o del gps.
#include <SPI.h>			//necesario para comunicarse con el modulo micro sd
#include <SD.h>				//necesario para comunicarse con el modulo micro sd
#include <ArduinoLog.h>		//útil para debugear por niveles de errores
#include <SoftwareSerial.h>	//necesaria para la comunicación con los módulos sim

#define SIM_BAUDRATE 9600    	///< Control the baudrate use to communicate with the SIM808 module
#define SERIAL_BAUDRATE 38400   ///< Controls the serial baudrate between the arduino and the computer
#define NETWORK_DELAY  10000    ///< Delay between each GPS read & network restart
#define CMD_DELAY  2500	

#define SIM_RST   6 ///< SIM RESET
#define SIM_RX    9 ///< SIM RXD
#define SIM_TX    8 ///< SIM TXD

#if defined(USE_PIN)
	const char SIM_PIN[]="4217";	//código pin de la tarjeta SIM
#endif

const char APN[] = "internet";
	
#if defined(USE_SIM808)
	#include <SIM808.h>
	#define SIM_PWR   7 ///< SIM808 PWRKEY
	//gps:
	#define NO_FIX_GPS_DELAY 3000   ///< Delay between each GPS read when no fix is acquired
	#define FIX_GPS_DELAY  10000    ///< Delay between each GPS read when a fix is acquired
	#define POSITION_SIZE   128     ///< Size of the position buffer
	typedef __FlashStringHelper* __StrPtr;
#else
	#include "SIM800L.h"
#endif

#define BUFFER_SIZE 512         ///< Side of the response buffer
#define NL  "\n"


////SoftwareSerial simSerial = SoftwareSerial(SIM_TX, SIM_RX);
////SIM808 sim808 = SIM808(SIM_RST, SIM_PWR);//, SIM_STATUS);

////char position[POSITION_SIZE];

////char buffer[BUFFER_SIZE];
////char post_msg_buffer[BUFFER_SIZE];
////char post_msg[BUFFER_SIZE];

void setup() {
	
	delay(6000);//delay de arranque

    Serial.begin(SERIAL_BAUDRATE);
    Log.begin(LOG_LEVEL_NOTICE, &Serial);

    simSerial.begin(SIM_BAUDRATE);
    sim808.begin(simSerial);

    Log.notice(S_F("Powering on SIM808..." NL));
    sim808.powerOnOff(true);
    sim808.init();    

    Log.notice(S_F("Powering on SIM808's GPS..." NL));
    sim808.powerOnOffGps(true);
}

void loop() {
	
delay(CMD_DELAY);

//gps
    SIM808GpsStatus status_gps = sim808.getGpsStatus(position, POSITION_SIZE);
    
    if(status_gps < SIM808GpsStatus::Fix) {
        Log.notice(S_F("No fix yet..." NL));
        delay(NO_FIX_GPS_DELAY);
        return;
    }

//    uint16_t sattelites;
    float lat, lon;
    __StrPtr state;

    if(status_gps == SIM808GpsStatus::Fix) state = S_F("Normal");
    else state = S_F("Accurate");

    sim808.getGpsField(position, SIM808GpsField::Latitude, &lat);
    sim808.getGpsField(position, SIM808GpsField::Longitude, &lon);

delay(CMD_DELAY);

//sim
    SIM808NetworkRegistrationState status = sim808.getNetworkRegistrationStatus();
delay(CMD_DELAY);				 
    SIM808SignalQualityReport report = sim808.getSignalQuality();
delay(CMD_DELAY);
// añadimos el registro del pin de la sim:
    bool success = sim808.simUnlock(SIM_PIN);
    if(success)
       Log.notice(S_F("SIM unlock : pin registered :)" NL));
    else
       Log.notice(S_F("failed registering pin (maybe already registered?)" NL));
       
delay(NETWORK_DELAY);														
    bool isAvailable = static_cast<int8_t>(status) &
        (static_cast<int8_t>(SIM808NetworkRegistrationState::Registered) | static_cast<int8_t>(SIM808NetworkRegistrationState::Roaming))
        != 0;

    if(!isAvailable) {
        Log.notice(S_F("No network yet..." NL));
        delay(NETWORK_DELAY);
        return;
    }

    Log.notice(S_F("Network is ready." NL));
    Log.notice(S_F("Attenuation : %d dBm, Estimated quality : %d" NL), report.attenuation, report.rssi);

    bool enabled = false;
  do {
        Log.notice(S_F("Powering on SIM808's GPRS..." NL));
        enabled = sim808.enableGprs(APN, NULL, NULL); //apn,user,pass
delay(CMD_DELAY); 
delay(CMD_DELAY); 					
    } while(!enabled);
    
        Log.notice(S_F("Sending HTTP request..." NL));

// post con campos como get (urlencoded) ó en líneas separadas:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/POST
        
NO ESTÁ TOMANDO BIEN LOS VALORES DEL LAT Y LON AL TRANSFORMARLOS O UNIRLOS

        strlcpy_P(post_msg, PSTR("msg=sim808post"), BUFFER_SIZE);
        strcat(post_msg, PSTR("&lat="));
          sprintf(post_msg_buffer,"%f",lat);//dtostrf(lat,8,2,post_msg_buffer);
          strcat(post_msg, post_msg_buffer );
        strcat(post_msg, PSTR("&lon="));
          sprintf(post_msg_buffer,"%f",lon);
          strcat(post_msg, post_msg_buffer );
        uint16_t responseCode = sim808.httpPost("http://post.meph.website/?msg=sim808GET", S_F("application/x-www-form-urlencoded"), post_msg, buffer, BUFFER_SIZE);
    Log.notice(S_F("Server responsed : %d" NL), responseCode);
    Log.notice(buffer);

sim808.reset(); //sim808.powerOnOff(false);    //apagamos?
Log.notice("---END---");

while(1==1);
//ver como poner en modo sleep con esta librería o meterle el at directamente
}
