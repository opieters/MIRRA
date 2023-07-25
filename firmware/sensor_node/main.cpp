#include "config.h"
#include "sensornode.h"

void setup(void)
{
    constexpr MIRRAModule::MIRRAPins pins = {.boot_pin = BOOT_PIN,
                                             .sda_pin = SDA_PIN,
                                             .scl_pin = SCL_PIN,
                                             .cs_pin = CS_PIN,
                                             .rst_pin = RST_PIN,
                                             .dio0_pin = DIO0_PIN,
                                             .tx_pin = TX_PIN,
                                             .rx_pin = RX_PIN,
                                             .rtc_int_pin = RTC_INT_PIN,
                                             .rtc_address = RTC_ADDRESS};
    MIRRAModule::prepare(pins);
    SensorNode sensorNode = SensorNode(pins);
    sensorNode.wake();
}

void loop(void) {}
