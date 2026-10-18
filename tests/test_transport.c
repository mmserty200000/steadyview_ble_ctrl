#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "../STM32_WPAN/App/control_service.c"
#include "../Core/Src/control_app.c"
static uint8_t queue_storage[8][sizeof(ControlPacket)];
static unsigned head,tail,count,processed,notified,delays;
static uint8_t received[16],last_att_error;
static bool busy_once;
static jmp_buf done;
tBleStatus aci_l2cap_connection_parameter_update_req(uint16_t h,uint16_t min,uint16_t max,uint16_t latency,uint16_t timeout) {
    assert(h==7 && min==40 && max==64 && latency==0 && timeout==400);return 0;
}
void Error_Handler(void) { abort(); }
osMessageQueueId_t osMessageQueueNew(uint32_t n,uint32_t size,const void *attr) {
    (void)attr;assert(n==8 && size==sizeof(ControlPacket));return queue_storage;
}
int osMessageQueuePut(osMessageQueueId_t id,const void *msg,uint8_t p,uint32_t timeout) {
    assert(id==queue_storage && p==0 && timeout==0);
    if(count==8)return -1;
    memcpy(queue_storage[tail++%8],msg,sizeof(ControlPacket));++count;return osOK;
}
int osMessageQueueGet(osMessageQueueId_t id,void *msg,uint8_t *p,uint32_t timeout) {
    (void)id;(void)p;assert(timeout==osWaitForever);
    if(!count)longjmp(done,1);
    memcpy(msg,queue_storage[head++%8],sizeof(ControlPacket));--count;return osOK;
}
int osDelay(uint32_t ticks) { assert(ticks==10);++delays;return 0; }
void ControlProtocol_Init(void) {}
void ControlProtocol_Session(uint32_t session) {(void)session;}
uint32_t osKernelGetTickCount(void){return 0;}
uint32_t osKernelGetTickFreq(void){return 1000;}
bool ControlProtocol_Process(const uint8_t *data,size_t n,uint8_t *out,size_t *length) {
    assert(n==1 || n==35);for(size_t i=1;i<n;++i)assert(data[i]==data[0]);received[processed++]=data[0];memset(out,data[0],35);*length=35;return true;
}
tBleStatus aci_gatt_add_service(uint8_t type,const Service_UUID_t *uuid,uint8_t primary,uint8_t records,uint16_t *h) {
    assert(type==2 && uuid->bytes[0]==0x1a && primary==1 && records==6);*h=1;return 0;
}
tBleStatus aci_gatt_add_char(uint16_t service,uint8_t type,const Char_UUID_t *uuid,uint16_t len,uint8_t props,uint8_t sec,uint8_t events,uint8_t key,uint8_t variable,uint16_t *h) {
    (void)sec;(void)key;assert(service==1 && type==2 && len==20 && variable);
    if(uuid->bytes[0]==0x1b) { assert(props==CHAR_PROP_WRITE && events==2);*h=2; }
    else { assert(uuid->bytes[0]==0x1c && props==CHAR_PROP_NOTIFY && events==1);*h=4; }
    return 0;
}
void SVCCTL_RegisterSvcHandler(SVC_CTL_p_EvtHandler_t handler) { assert(handler==event_handler); }
tBleStatus aci_gatt_permit_write(uint16_t c,uint16_t attr,uint8_t status,uint8_t err,uint8_t len,const uint8_t *data) {
    (void)c;(void)data;(void)len;assert(attr==3 && status==(err!=0));last_att_error=err;return 0;
}
tBleStatus aci_gatt_update_char_value_ext(uint16_t c,uint16_t s,uint16_t ch,uint8_t type,uint16_t total,uint16_t offset,uint8_t n,const uint8_t *data) {
    assert(c==7 && s==1 && ch==4 && type==1 && total==n && offset==0);
    assert(data[0]==0xa1 && data[3]==3 && data[2]<3);
    assert(n==(data[2]==2 ? 7 : 20));
    if(busy_once) { busy_once=false;return BLE_STATUS_INSUFFICIENT_RESOURCES; }
    for(unsigned i=4;i<n;++i)assert(data[i]==received[notified]);
    if(data[2]==2)++notified;
    return 0;
}
tBleStatus hci_disconnect(uint16_t c,uint8_t reason) { (void)c;(void)reason;abort(); }
static void subscribe(bool enabled) {
    hci_uart_pckt packet={0};hci_event_pckt *h=(void *)packet.data;h->evt=0xff;
    evt_blecore_aci *a=(void *)h->data;a->ecode=ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE;
    aci_gatt_attribute_modified_event_rp0 *e=(void *)a->data;
    e->Connection_Handle=7;e->Attr_Handle=6;e->Attr_Data_Length=2;e->Attr_Data[0]=enabled;
    assert(event_handler(&packet)==SVCCTL_EvtAckFlowEnable);
}
static void write_packet(uint8_t value,uint8_t length) {
    hci_uart_pckt packet={0};hci_event_pckt *h=(void *)packet.data;h->evt=0xff;
    evt_blecore_aci *a=(void *)h->data;a->ecode=ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE;
    aci_gatt_write_permit_req_event_rp0 *e=(void *)a->data;
    e->Connection_Handle=7;e->Attribute_Handle=3;e->Data_Length=length;e->Data[0]=value;
    assert(event_handler(&packet)==SVCCTL_EvtAckFlowEnable);
    memset(&packet,0,sizeof(packet)); // queue must own the copy
}
static void drain(void) { if(setjmp(done)==0)ControlApp_Run(); }
int main(void) {
    ControlApp_Init();ControlService_Init();ControlService_Connected(7);
    write_packet(0,1);assert(last_att_error==0x80 && !count);
    subscribe(true);
    write_packet(0,0);assert(last_att_error==0x82);
    write_packet(0,21);assert(last_att_error==0x82);
    for(unsigned i=0;i<8;++i){write_packet(i,1);assert(last_att_error==0);}
    write_packet(9,1);assert(last_att_error==0x81 && count==8);
    busy_once=true;drain();assert(processed==8 && notified==8 && delays==1);
    for(unsigned i=0;i<8;++i)assert(received[i]==i);
    write_packet(99,1);ControlService_Disconnected(7);ControlService_Connected(7);subscribe(true);
    drain();assert(processed==8); // old packet must not execute after reconnect
    write_packet(99,1);subscribe(false);subscribe(true);drain();assert(processed==8);
    uint8_t frame[20]={0xa2,7,0,3};memset(frame+4,0x5a,16);
    assert(ControlApp_Enqueue(generation,frame,20));
    frame[2]=1;assert(ControlApp_Enqueue(generation,frame,20));
    frame[2]=2;assert(ControlApp_Enqueue(generation,frame,7));
    drain();assert(processed==9 && received[8]==0x5a && notified==9);
    // Missing/out-of-order fragments and an oversized count never reach the decoder.
    frame[2]=0;assert(ControlApp_Enqueue(generation,frame,20));
    frame[2]=2;assert(ControlApp_Enqueue(generation,frame,7));
    frame[2]=0;frame[3]=9;assert(ControlApp_Enqueue(generation,frame,20));
    drain();assert(processed==9);
    puts("FIFO/BLE transport: passed");
}
