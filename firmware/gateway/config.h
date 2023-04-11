#ifndef __CONFIG_H__
#define __CONFIG_H__

#define TINY_GSM_MODEM_SIM800

#define M_SEL0 25
#define M_SEL1 27

// Pin assignments

#define BOOT_PIN 1

#define SDA_PIN 21
#define SCL_PIN 22

// LoRa module pins
#define CS_PIN 13   // Chip select pin
#define RST_PIN 15  // Reset pin
#define DIO0_PIN 34 // DIO0 pin: LoRa interrupt pin
#define DIO1_PIN 4  // DIO1 pin: not used??
#define TX_PIN 17
#define RX_PIN 16

// PCF2129 timer
#define RTC_INT_PIN 35   // Interrupt pin
#define RTC_ADDRESS 0x51 // i2c address

// WiFi settings
#define SSID "GontrodeWiFi2"
#define PASS "b5uJeswA"
#define NTP_URL "be.pool.ntp.org"
// MQTT settings
#define MQTT_SERVER IPAddress(193, 190, 127, 143)
#define MQTT_PORT 1883
#define TOPIC_PREFIX "fornalab" // MQTT topic = `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`

// Communication and sensor settings (seconds)
#define COMMUNICATION_PERIOD_LENGTH
#define COMMUNICATION_PERIOD_PADDING
#define COMMUNICATION_INTERVAL
#define SAMPLING_INTERVAL

#define MAX_SENSOR_NODES

#endif