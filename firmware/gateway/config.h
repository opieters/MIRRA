#ifndef __CONFIG_H__
#define __CONFIG_H__

#define TINY_GSM_MODEM_SIM800

#define M_SEL0 25
#define M_SEL1 27

// Pin assignments
#define BOOT_PIN 0

#define PERIPHERAL_POWER_PIN 16

#define SDA_PIN 21
#define SCL_PIN 22

// LoRa module pins
#define CS_PIN 13   // Chip select pin
#define RST_PIN 14  // Reset pin
#define DIO0_PIN 34 // DIO0 pin: LoRa interrupt pin
#define TX_PIN 17
#define RX_PIN 16

// PCF2129 timer
#define RTC_INT_PIN 35   // Interrupt pin
#define RTC_ADDRESS 0x51 // i2c address

// Filepaths
#define NODES_FP "/nodes.dat"
#define DATA_FP "/data.dat"
#define DATA_TEMP_FP "/data_temp.dat"

// WiFi settings
#define WIFI_SSID "GontrodeWiFi2"
#define WIFI_PASS "b5uJeswA"
#define NTP_URL "be.pool.ntp.org"

// MQTT settings
#define MQTT_SERVER IPAddress(5, 9, 199, 28)
#define MQTT_PORT 1883
#define TOPIC_PREFIX "fornalab" // MQTT topic = `TOPIC_PREFIX` + '/' + `GATEWAY MAC` + '/' + `SENSOR MODULE MAC`
#define MQTT_ATTEMPTS 5         // amount of attempts made to connect to MQTT server
#define MQTT_TIMEOUT 1000       // ms, timeout to connect with MQTT server
#define MAX_MQTT_ERRORS 3       // max number of MQTT publish errors after which uploading should be aborted

// Communication and sensor settings

// s, time between each node's communication period
#define COMM_PERIOD_PADDING 3
// s, time between communication times for every nodes
#define DEFAULT_COMM_INTERVAL (60 * 60)

#define WAKE_BEFORE_COMM_PERIOD 5 // s, time before comm period when gateway should wake from deep sleep
#define WAKE_COMM_PERIOD(X) ((X)-WAKE_BEFORE_COMM_PERIOD)
#define LISTEN_COMM_PERIOD(X) ((X)-COMM_PERIOD_PADDING)

#define UPLOAD_EVERY 3 // amount of times the gateway will communicate with the nodes before uploading data to the server

#define DEFAULT_SAMPLE_INTERVAL (20 * 60) // s, time between sensor sampling for every node
#define DEFAULT_SAMPLE_ROUNDING (20 * 60) // s, round sampling time to nearest ...
#define DEFAULT_SAMPLE_OFFSET (0)

#define DISCOVERY_TIMEOUT 5000 // ms

#define TIME_CONFIG_TIMEOUT 6000 // ms
#define TIME_CONFIG_ATTEMPTS 1

#define SENSOR_DATA_TIMEOUT 6000 // ms
#define SENSOR_DATA_ATTEMPTS 1

#define MAX_SENSORDATA_FILESIZE 128 * 1024 // bytes

#define MAX_SENSOR_NODES 20

#endif
