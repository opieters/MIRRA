#include <Arduino.h>
#include <time.h>

#include "UplinkModule.h"
#include <config.h>

/********************************************
CONSTRUCTORS
********************************************/
/**
 * @brief Construct a new GPRSHelper object
 * 
 * @param stream The HardwareSerial used communicate with the GPRS module
 */
UplinkModule::UplinkModule(Stream & stream, uint8_t reset_pin) {
    this->modem = new TinyGsm(stream);
    this->reset_pin = reset_pin;
}

/**
 * @brief Construct a new GPRSHelper object
 * 
 * @param sim The 
 */
UplinkModule::UplinkModule(const TinyGsmSim800 & sim, uint8_t reset_pin){
    this->modem = new TinyGsm(sim);
    this->reset_pin = reset_pin;
}

/**
 * @brief Construct a new GPRSHelper object
 * 
 * Requires debug level 2
 * 
 * @param debugger Debugger wrapper around the harwareSerial al the 
 *      commands sent to the HardwareSerial and its responses will be 
 *      shown in the Serial monitor.
 */
UplinkModule::UplinkModule(StreamDebugger & debugger, uint8_t reset_pin) {
    this->modem = new TinyGsm(debugger);
    this->reset_pin = reset_pin;
}


/****************************************************
FUNCTIONS
****************************************************/
bool UplinkModule::startGPRS() {
    static uint32_t retry_count = 0;

    if(!modem->isNetworkConnected()){
        Serial.println("Initialising modem");
        reset();
        //modem->restart();
        Serial.print("Waiting for module reboot during ");
        Serial.print(30 * (retry_count + 1));
        Serial.println("s.");
        delay(30*1000 * (retry_count + 1));
    }
    

    String modemInfo = modem->getModemInfo();
    Serial.print("Modem Info: ");
    Serial.println(modemInfo);

    Serial.print("SIM status:");
    Serial.println(modem->getSimStatus());

    Serial.print("Waiting for network...");
    modem->waitForNetwork();

    if(!modem->isNetworkConnected()){
        Serial.println(" fail.");
        retry_count++;
        //return false;
    } else {
        Serial.println(" success.");
        Serial.print("Modem info: ");
        modemInfo = modem->getModemInfo();
        Serial.println(modemInfo);
        Serial.print("RSSI:");
        Serial.println(modem->getSignalQuality());
    }

    if(retry_count > 0){
        Serial.print("Waiting for module during");
        Serial.print(10 * retry_count);
        Serial.println("s.");
        delay(10*1000 * retry_count);
    }

    Serial.print("Connecting to... ");
    Serial.println(APN);

    if(!modem->gprsConnect(APN, USER, PASS)){
        Serial.println("GPRS connection to provider failed.");
        retry_count++;
    } else {
        Serial.println("Succes!");
        retry_count = 0;
        return true;
    }

    return false;
}
bool UplinkModule::GPRSConnectAPN(){
    #if DEBUG 
        Serial.printf("  * Connecting to %s ", APN);
        bool connected = false;
        uint16_t attempt_counter = 100;
        while ((!connected) && (attempt_counter > 0)){
            connected = TinyGsm::gprsConnect(APN, USER, PASS);
            Serial.print("...");
        }
        if(connected){
        Serial.println(" OK");
        Serial.printf("RSSI; %d\n", TinyGsm::getSignalQuality())   ;
        }
        return connected;
    #else
        return modem->gprsConnect(APN);
    #endif
}


uint32_t UplinkModule::get_rtc_time(){
    //!source: https://cdn-shop.adafruit.com/product-files/2637/SIM800+Series_NTP_Application+Note_V1.01.pdf
    //! Configure the Bearer
    // TinyGsm::sendAT("+SAPBR=1,1");
    modem->sendAT("+CNTPCID=1");
    // TinyGsm::waitResponse(2000L, GSM_OK);

    //! Set the NTP server
    modem->sendAT("+CNTP=\"" + String(NTP_SERVER) + "\",4");
    //TinyGsm::waitResponse(2000L, GSM_OK);

    //! Start the NTP service (Synchronize network time)
    modem->sendAT("+CNTP");
    modem->waitResponse(GSM_OK);
    delay(4000);

    //! Retrieve the Current Date and store it in an char [] (executes AT+CCLK command)
    String date_str = modem->getGSMDateTime(DATE_FULL);
    char date_char[21];
    date_str.toCharArray(date_char, 21);

    //! Create a time struct in which we will store the components from the date char
    struct tm t;
    t.tm_year = 2000 - DFLT_YEAR + CHAR2NUMBVAL(date_char[0]) * 10 + CHAR2NUMBVAL(date_char[1]); 
    t.tm_mon = CHAR2NUMBVAL(date_char[3]) * 10 + CHAR2NUMBVAL(date_char[4]) -1 ;
    t.tm_mday = CHAR2NUMBVAL(date_char[6]) * 10 + CHAR2NUMBVAL(date_char[7]);

    t.tm_hour = CHAR2NUMBVAL(date_char[9]) * 10 + CHAR2NUMBVAL(date_char[10]);
    t.tm_min =  CHAR2NUMBVAL(date_char[12]) * 10 + CHAR2NUMBVAL(date_char[13]);

    //! We add 2 seconds, since there is a delay of 2 seconds after sending the CNTP command
    t.tm_sec = CHAR2NUMBVAL(date_char[15]) * 10 + CHAR2NUMBVAL(date_char[16]) + 4;


    //! Also take the minute offset into account

    int minute_offset = -1;
    if (date_char[17] == '+'){
        minute_offset = 1;
    }
    //! the retrieved minute offset from the sim800L its resolution in 15 minutes
    minute_offset *= 15 * (CHAR2NUMBVAL(date_char[18]) * 10 + CHAR2NUMBVAL(date_char[19]));

    time_t posix_time;
    posix_time = mktime(&t);

    // char buf [80];
    // strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &t);
    // Serial.printf("formatted struct: %s \n", buf);

    #if DEBUG
        Serial.println("  GPRS: Date str: " + date_str);
        Serial.printf("  * Extracted date: %d/%d/%d - %d:%d:%d \n", t.tm_year + DFLT_YEAR, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        Serial.printf("  * Offset: %d min \n", minute_offset);
        Serial.printf("    - Posix time: %d \n", (uint32_t) posix_time - 60 * minute_offset);
    #endif

    //! Calculate the total number of days
    return (uint32_t) posix_time - 60 * minute_offset;
}

void UplinkModule::reset(void){
    pinMode(this->reset_pin, OUTPUT);
    digitalWrite(this->reset_pin, LOW);

    delay(150); // at least 100ms

    digitalWrite(this->reset_pin, HIGH);
    delay(100);
}


void UplinkModule::sleep(void){
    modem->radioOff();
    modem->poweroff();
    //modem->sendAT("AT+CPOWD=0");
}
