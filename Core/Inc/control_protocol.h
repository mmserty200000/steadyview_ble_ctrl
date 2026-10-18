#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CONTROL_PACKET_MAX 20U
#define CONTROL_REQUEST_MAX 128U
#define CONTROL_RESPONSE_MAX 512U
void ControlProtocol_Init(void);
void ControlProtocol_Session(uint32_t session);
bool ControlProtocol_Process(const uint8_t *data, size_t length,
                             uint8_t *response, size_t *response_length);
#endif
