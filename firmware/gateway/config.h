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

// Communication and sensor settings 
#define COMMUNICATION_PERIOD_LENGTH 20 //s, time reserved for communication between gateway and one node
#define COMMUNICATION_PERIOD_PADDING 5 //s, time between each node's communication period
#define COMMUNICATION_INTERVAL 60*60*24 //s, time between communication times for every nodes

#define UPLOAD_EVERY 3 // amount of times the gateway will communicate with the nodes before uploading data to the server

#define SAMPLING_INTERVAL 60*60*3 //s, time between sensor sampling for every node
#define SAMPLING_ROUNDING 60*60 //s, round sampling time to nearest ...

#define DISCOVERY_TIMEOUT 5000 //ms
#define TIME_CONFIG_TIMEOUT 3000 //ms
#define TIME_CONFIG_ATTEMPTS 3

#define MAX_SENSORDATA_FILESIZE 1024 * 1024 //bytes

#define MAX_SENSOR_NODES 20

#endif