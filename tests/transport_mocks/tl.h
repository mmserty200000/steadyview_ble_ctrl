#ifndef MOCK_TL_H
#define MOCK_TL_H
#include <stdint.h>
typedef struct { uint8_t type; uint8_t data[260]; } hci_uart_pckt;
typedef struct { uint8_t evt,plen; uint8_t data[258]; } hci_event_pckt;
typedef struct __attribute__((packed)) { uint16_t ecode; uint8_t data[256]; } evt_blecore_aci;
#endif
