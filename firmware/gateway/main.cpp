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


volatile bool commandPhaseEntry = false;

void IRAM_ATTR gpio_0_isr_callback()
{
    commandPhaseEntry = true;
}

void setup(void)
{
    pinMode(0, INPUT);
    attachInterrupt(0, gpio_0_isr_callback, FALLING);

    Serial.begin(115200);
    Serial.println("Serial initialised");
    Serial2.begin(9600);
    Wire.begin(SDA_PIN, SCL_PIN); // i2c
    Serial.println("I2C wire initialised");

    PCF2129_RTC rtc= PCF2129_RTC(RTC_INT_PIN, RTC_ADDRESS);
    Serial.println("RTC initialised");
    Logger base_log = Logger(Logger::debug, LOG_FP, &rtc);
    Serial.println("Logger initialised");
    Gateway gateway = Gateway(&base_log, &rtc);
    Serial.println("Gateway initialised");

    gateway.wake();
}

void loop(void)
{

}
