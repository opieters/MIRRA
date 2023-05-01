/****************************************************
LIBRARIES
****************************************************/

// #define USE_WIFI

#include "config.h"
#include "utilities.h"
#include "UplinkModule.h"
#include "PubSubClient.h"
#include "PCF2129_RTC.h" //!< External RTC module
#include <cstring>
#include "SPIFFS.h"
#include "WiFi.h"
#include "gateway.h"
#include "CommunicationCommon.h"
#include "logging.h"

// TinyGsmClient client(*gsmModule.modem);
// PubSubClient mqtt(mqttServer, 1883, client);

PCF2129_RTC rtc= PCF2129_RTC(RTC_INT_PIN, RTC_ADDRESS);
Logger base_log = Logger(Logger::debug, LOG_FP, &rtc);
Gateway gateway = Gateway(&base_log, &rtc);

volatile bool commandPhaseEntry = false;

void IRAM_ATTR gpio_0_isr_callback()
{
    commandPhaseEntry = true;
}

void setup(void)
{
    pinMode(0, INPUT);
    attachInterrupt(0, gpio_0_isr_callback, FALLING);
    gateway.wake();
}

void loop(void)
{

}
