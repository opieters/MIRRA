/*
 * Define GSM module for library
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define TINY_GSM_MODEM_SIM800

#define MAC_BYTES           6
#define PAYLOAD_LEN_BYTES   1
#define TIMESTAMP_BYTES     4

#define receiveWindow 200
#define receiveDataWindow 1000
#define receiveAckWindow 10

/* PCF2129 timer */
#define RTC_INT_PIN         35       //!< Interrupt pin
#define RTC_ADDRESS         0x51    //!< The PCF2129 its i2c address

#define M_SEL0  25
#define M_SEL1  27


#endif