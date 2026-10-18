#ifndef MOCK_OS_H
#define MOCK_OS_H
#include <stdint.h>
#include <stddef.h>
typedef void *osMessageQueueId_t;
#define osOK 0
#define osWaitForever 0xffffffffU
osMessageQueueId_t osMessageQueueNew(uint32_t count,uint32_t size,const void *attr);
int osMessageQueuePut(osMessageQueueId_t id,const void *msg,uint8_t priority,uint32_t timeout);
int osMessageQueueGet(osMessageQueueId_t id,void *msg,uint8_t *priority,uint32_t timeout);
int osDelay(uint32_t ticks);
#endif

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickFreq(void);
