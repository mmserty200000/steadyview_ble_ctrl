#ifndef CONTROL_APP_H
#define CONTROL_APP_H
#include <stdbool.h>
#include <stdint.h>
#include "control_protocol.h"
void ControlApp_Init(void);
bool ControlApp_Enqueue(uint32_t session, const uint8_t *data, uint8_t length);
void ControlApp_Run(void);
#endif
