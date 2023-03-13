#include <Arduino.h>
#include "utilities.h"
#include "config.h"

static uint8_t macAddress[6];

void initModuleSelector(void){
    pinMode(M_SEL0, OUTPUT);
    pinMode(M_SEL1, OUTPUT);
}
void selectModule(ModuleSelector module){
    switch(module){
        case ModuleSelector::LoRaModule:
            digitalWrite(M_SEL0, LOW);
            digitalWrite(M_SEL1, HIGH);
            break;
        case ModuleSelector::GPRSModule:
            digitalWrite(M_SEL0, LOW);
            digitalWrite(M_SEL1, HIGH);
            break;
    }
}

uint8_t* getMacAddress(void){
    // TODO
    return macAddress;
}