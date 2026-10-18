#include "control_app.h"
#include "control_service.h"
#include "steadyview.h"
#include "cmsis_os2.h"
#include "main.h"
#include <string.h>

typedef struct {
    uint32_t session;
    uint8_t length;
    uint8_t data[CONTROL_PACKET_MAX];
} ControlPacket;
static osMessageQueueId_t rx_fifo;
void ControlApp_Init(void)
{
    rx_fifo = osMessageQueueNew(8, sizeof(ControlPacket), NULL);
    if (rx_fifo == NULL) Error_Handler();
}
bool ControlApp_Enqueue(uint32_t session, const uint8_t *data, uint8_t length)
{
    if (!rx_fifo || !length || length > CONTROL_PACKET_MAX) return false;
    ControlPacket packet = {.session = session, .length = length};
    memcpy(packet.data, data, length); // Stack event buffer expires after callback.
    return osMessageQueuePut(rx_fifo, &packet, 0, 0) == osOK;
}
void ControlApp_Run(void)
{
    ControlProtocol_Init();
    uint8_t transfer_id=0, rx_id=0, rx_index=0, rx_count=0;
    uint8_t request[CONTROL_REQUEST_MAX];
    size_t request_length=0;
    uint32_t session=0, rx_tick=0;
    for (;;) {
        ControlPacket packet;
        uint8_t response[CONTROL_RESPONSE_MAX];
        size_t length;
        if (osMessageQueueGet(rx_fifo, &packet, NULL, osWaitForever) != osOK) continue;
        if (!ControlService_IsCurrent(packet.session)) continue;
        if(session!=packet.session) {
            session=packet.session;rx_count=0;request_length=0;
            ControlProtocol_Session(session);
        }
        const uint8_t *input=packet.data;
        size_t input_length=packet.length;
        if(packet.data[0]==0xa2) {
            uint32_t now=osKernelGetTickCount();
            if((uint32_t)(now-rx_tick)>5U*osKernelGetTickFreq())rx_count=0;
            if(packet.length<5 || packet.data[3]==0 || packet.data[3]>CONTROL_REQUEST_MAX/16) {
                rx_count=0;continue;
            }
            if(packet.data[2]==0) {
                rx_id=packet.data[1];rx_count=packet.data[3];rx_index=0;request_length=0;
            }
            if(!rx_count || packet.data[1]!=rx_id || packet.data[2]!=rx_index ||
               packet.data[3]!=rx_count || rx_index>=rx_count ||
               (rx_index+1<rx_count && packet.length!=20) ||
               request_length+packet.length-4>sizeof(request)) {rx_count=0;continue;}
            memcpy(request+request_length,packet.data+4,packet.length-4);
            request_length+=packet.length-4;rx_tick=now;
            if(++rx_index<rx_count)continue;
            input=request;input_length=request_length;rx_count=0;
        } else rx_count=0;
        if (!ControlProtocol_Process(input, input_length, response, &length)) Error_Handler();
        // Each notification has a four-byte transport header and up to 16 bytes of protobuf.
        uint8_t count=(uint8_t)((length+15)/16);
        ++transfer_id;
        for (uint8_t index=0;index<count;++index) {
            uint8_t frame[CONTROL_PACKET_MAX]={0xa1,transfer_id,index,count};
            size_t offset=(size_t)index*16;
            size_t chunk=length-offset;
            if(chunk>16)chunk=16;
            memcpy(frame+4,response+offset,chunk);
            int result;
            // Retain the exact fragment on resource exhaustion; never re-execute the request.
            while((result=ControlService_Send(packet.session,frame,(uint8_t)(chunk+4)))==1)osDelay(10);
            if(result<0)break;
        }
    }
}
