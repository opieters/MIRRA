#ifndef __CONFIG_H__
#define __CONFIG_H__
// Pin assignments

#define BOOT_PIN 0

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

// UART settings
#define UART_PHASE_ENTRY_PERIOD 15  // s, length of time after wakeup and comm period in which command phase can be entered
#define UART_PHASE_TIMEOUT (5 * 60) // s, length of UART inactivity required to automatically exit command phase

// Communication and sensor settings

#define DEFAULT_SAMPLING_INTERVAL (60 * 60)//s, default sensor sampling interval to resort to when no communication with gateway is established
#define SAMPLING_ROUNDING (60 * 60) // s, round sampling time to nearest... (only used in case of DEFAULT_SAMPLING_INTERVAL)

#define DISCOVERY_TIMEOUT 5 * 60 * 1000 //ms, time to wait for a discovery message from gateway

#define TIME_CONFIG_TIMEOUT 10000 // ms
#define TIME_CONFIG_ATTEMPTS 3

#define SENSOR_DATA_TIMEOUT 10000 // ms
#define SENSOR_DATA_ATTEMPTS 3

#define MAX_SENSORDATA_FILESIZE 32 * 1024 // bytes

#endif
