#ifndef CONTROL_SERVICE_H
#define CONTROL_SERVICE_H
#include <stdbool.h>
#include <stdint.h>
extern const uint8_t ControlService_UUID[16];
void ControlService_Init(void);
void ControlService_Connected(uint16_t connection);
void ControlService_Disconnected(uint16_t connection);
bool ControlService_IsCurrent(uint32_t session);
/* 0 sent; 1 transient, retry; -1 stale session or permanent failure. */
int ControlService_Send(uint32_t session, const uint8_t *data, uint8_t length);
#endif
