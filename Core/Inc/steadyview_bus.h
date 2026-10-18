#ifndef STEADYVIEW_BUS_H
#define STEADYVIEW_BUS_H
#include <stdbool.h>
#include <stdint.h>
bool SVBus_Write(uint8_t address, uint32_t value);
bool SVBus_Read(uint8_t address, uint32_t *value);
bool SVBus_MixPulse(void);
void SVBus_DelayMs(uint32_t ms);
void SVBus_DelayUs(uint32_t us);
#endif
