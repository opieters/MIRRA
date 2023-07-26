#ifndef __CONFIG_H__
#define __CONFIG_H__
// Pin assignments

#define BOOT_PIN 0

#define PERIPHERAL_POWER_PIN 16

#define SDA_PIN 21
#define SCL_PIN 22

// LoRa module pins
#define CS_PIN 27   // Chip select pin
#define RST_PIN 26  // Reset pin
#define DIO0_PIN 34 // DIO0 pin: LoRa interrupt pin
#define TX_PIN 13
#define RX_PIN 25

// PCF2129 timer
#define RTC_INT_PIN 4    // Interrupt pin
#define RTC_ADDRESS 0x51 // i2c address

// Filepaths
#define LOG_FP "/"
#define DATA_FP "/data.dat"
#define DATA_TEMP_FP "/data_temp.dat"

// Communication and sensor settings
#define WAKE_BEFORE_COMM_PERIOD 3 // s, time before comm period when node should wake from deep sleep
#define WAKE_COMM_PERIOD(X) ((X)-WAKE_BEFORE_COMM_PERIOD)

#define DEFAULT_SAMPLING_INTERVAL (60 * 60) // s, default sensor sampling interval to resort to when no communication with gateway is established
#define DEFAULT_SAMPLING_ROUNDING (60)      // s, round sampling time to nearest... (only used in case of DEFAULT_SAMPLING_INTERVAL)
#define DEFAULT_SAMPLING_OFFSET (0)

#define DISCOVERY_TIMEOUT (5 * 60 * 1000) // ms, time to wait for a discovery message from gateway

#define TIME_CONFIG_TIMEOUT 6000 // ms
#define TIME_CONFIG_ATTEMPTS 1

#define SENSOR_DATA_TIMEOUT 6000 // ms
#define SENSOR_DATA_ATTEMPTS 1

#define MAX_SENSORDATA_FILESIZE 32 * 1024 // bytes
#define MAX_SENSORS 20

// Sensor pins

#define BATT_PIN 35
#define BATT_EN_PIN 33

#define CAM_PIN GPIO_NUM_2

#endif
