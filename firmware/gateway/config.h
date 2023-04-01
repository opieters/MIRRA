#ifndef __CONFIG_H__
#define __CONFIG_H__

#define TINY_GSM_MODEM_SIM800

/* PCF2129 timer */
#define RTC_INT_PIN 35   //!< Interrupt pin
#define RTC_ADDRESS 0x51 //!< The PCF2129 its i2c address

#define M_SEL0 25
#define M_SEL1 27

// Pin assignments
#define CS_PIN 13   // Chip select pin
#define RST_PIN 15  // Reset pin
#define DIO0_PIN 34 // DIO0 pin: LoRa interrupt pin
#define DIO1_PIN 4  // DIO1 pin: not used??
#define TX_PIN 17
#define RX_PIN 16

#endif