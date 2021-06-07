#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <stdint.h>

#define SECONDS_TO_US(X) ((X)*1000000)
#define ARRAY_LENGTH(X) (sizeof(X) / sizeof(X[0]))

enum class ModuleSelector {
    LoRaModule,
    GPRSModule,
};

void initModuleSelector(void);
void selectModule(ModuleSelector module);

uint8_t* getMacAddress(void);

#endif