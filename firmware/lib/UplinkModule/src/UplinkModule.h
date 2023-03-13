#ifndef __UPLINK_MODULE_H__
#define __UPLINK_MODULE_H__

#ifndef TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM800
#endif
#include <TinyGsmClient.h>
#include <StreamDebugger.h>


/****************************************************
CONSTANTS & MACROS
****************************************************/
#define DFLT_YEAR   1900

#define CHAR2NUMBVAL(x) (x-48L)

#define APN     "globaldata"        //!< Should be provided with the sim-card 
#define USER    ""                  //!< Leave empty if no username is required
#define PASS    ""                  //!< Leave empty if no password is needed

#define SerialAT        Serial2     //!< HardwareSerial, sending AT commands to GPRS module
#define BAUD_AT         115200

#define NTP_SERVER          "nl.pool.ntp.org"

/****************************************************
CLASS TEMPLATE
****************************************************/
class UplinkModule {
    private:
        

        uint8_t reset_pin;
    public: 
        TinyGsm* modem;

        /********************************************
        CONSTRUCTORS
        ********************************************/
        /**
         * @brief Construct a new GPRSHelper object
         * 
         * @param stream The HardwareSerial used communicate with the GPRS module
         */
        UplinkModule(Stream & stream, uint8_t pin);

        /**
         * @brief Construct a new GPRSHelper object
         * 
         * @param sim The 
         */
        UplinkModule(const TinyGsmSim800 & sim, uint8_t pin);
        
        /**
         * @brief Construct a new GPRSHelper object
         * 
         * Requires debug level 2
         * 
         * @param debugger Debugger wrapper around the harwareSerial al the 
         *      commands sent to the HardwareSerial and its responses will be 
         *      shown in the Serial monitor.
         */
        UplinkModule(StreamDebugger & debugger, uint8_t pin);

        
        /********************************************
        PUBLIC METHODS
        ********************************************/
        /**
         * @brief Initializes the GPRS module and checks the requirements
         * 
         * Mostly depends on the higher level configurations e.g.
         * if correctly configured: 
         *  * can check whether the modem is SSL compatible 
         * 
         */
        bool startGPRS();

        /**
         * @brief Waits until a network is available or a timeout is reached
         * 
         * @param timeout_s The timeout (in seconds) until the GPRS module will search
         *      for a network. 
         * 
         * @return true when networks were found.
         * @return false when no networks were found
         */
        bool waitGPRSNetwork(uint32_t timeout_s = 60);

        /**
         * @brief Tries to connect the GPRS module to the pre-configured APN
         * 
         * @return true When the connection succeeded
         * @return false If something went wrong whilst establishing the connection
         */
        bool GPRSConnectAPN();

        /**
         * @brief Get the rtc time via NTP request
         * 
         * @return uint32_t The posix time
         */
        uint32_t  get_rtc_time();

        void reset(void);

        void sleep(void);
};

#endif // !GPRS_HELPER_H