#ifndef MOCK_BLE_H
#define MOCK_BLE_H
#include <stdint.h>
typedef uint8_t tBleStatus;
typedef struct { uint8_t bytes[16]; } Service_UUID_t;
typedef Service_UUID_t Char_UUID_t;
typedef enum { SVCCTL_EvtNotAck, SVCCTL_EvtAckFlowEnable } SVCCTL_EvtAckStatus_t;
typedef SVCCTL_EvtAckStatus_t (*SVC_CTL_p_EvtHandler_t)(void *);
#define HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE 0xff
#define ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE 0x0c01
#define ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE 0x0c13
#define ACI_GATT_PREPARE_WRITE_PERMIT_REQ_VSEVT_CODE 0x0c18
typedef struct __attribute__((packed)) { uint16_t Connection_Handle,Attribute_Handle,Offset; uint8_t Data_Length,Data[249]; } aci_gatt_prepare_write_permit_req_event_rp0;
#define BLE_STATUS_SUCCESS 0
#define BLE_STATUS_INSUFFICIENT_RESOURCES 0x64
#define BLE_STATUS_BUSY 0x93
#define UUID_TYPE_128 2
#define PRIMARY_SERVICE 1
#define CHAR_PROP_WRITE 8
#define CHAR_PROP_NOTIFY 16
#define ATTR_PERMISSION_NONE 0
#define GATT_NOTIFY_ATTRIBUTE_WRITE 1
#define GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP 2
typedef struct __attribute__((packed)) { uint16_t Connection_Handle,Attr_Handle,Offset,Attr_Data_Length; uint8_t Attr_Data[248]; } aci_gatt_attribute_modified_event_rp0;
typedef struct __attribute__((packed)) { uint16_t Connection_Handle,Attribute_Handle; uint8_t Data_Length,Data[251]; } aci_gatt_write_permit_req_event_rp0;
tBleStatus aci_gatt_add_service(uint8_t,const Service_UUID_t *,uint8_t,uint8_t,uint16_t *);
tBleStatus aci_gatt_add_char(uint16_t,uint8_t,const Char_UUID_t *,uint16_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint16_t *);
tBleStatus aci_gatt_permit_write(uint16_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *);
tBleStatus aci_gatt_update_char_value_ext(uint16_t,uint16_t,uint16_t,uint8_t,uint16_t,uint16_t,uint8_t,const uint8_t *);
tBleStatus hci_disconnect(uint16_t,uint8_t);
tBleStatus aci_l2cap_connection_parameter_update_req(uint16_t,uint16_t,uint16_t,uint16_t,uint16_t);
void SVCCTL_RegisterSvcHandler(SVC_CTL_p_EvtHandler_t);
#endif
