#include "app_common.h"
#include "ble.h"
#include "tl.h"
#include "shci.h"
#include "control_service.h"
#include "control_app.h"
#include "main.h"

// Canonical service UUID: bad8dbcb-268e-4df5-aeef-f3bd1961161a
// RX/TX differ in the final byte: ...161b / ...161c. ST takes little-endian UUIDs.
const uint8_t ControlService_UUID[16] = {
    0x1a,0x16,0x61,0x19,0xbd,0xf3,0xef,0xae,0xf5,0x4d,0x8e,0x26,0xcb,0xdb,0xd8,0xba
};
static uint16_t service_handle, rx_handle, tx_handle;
static volatile uint16_t connection = 0xffff;
static volatile uint32_t generation;
static volatile bool subscribed;

static void check(tBleStatus status)
{
    if (status != BLE_STATUS_SUCCESS) Error_Handler();
}
void ControlService_Connected(uint16_t handle)
{
    subscribed = false;
    ++generation;
    connection = handle;
    /* Request idle windows long enough for a Flash page erase; central may decline. */
    (void)aci_l2cap_connection_parameter_update_req(handle, 40, 64, 0, 400);
}
void ControlService_Disconnected(uint16_t handle)
{
    if (handle == connection) {
        subscribed = false;
        connection = 0xffff;
        ++generation;
    }
}
bool ControlService_IsCurrent(uint32_t session)
{
    return connection != 0xffff && subscribed && generation == session;
}
static SVCCTL_EvtAckStatus_t event_handler(void *event)
{
    hci_event_pckt *hci = (hci_event_pckt *)((hci_uart_pckt *)event)->data;
    if (hci->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) return SVCCTL_EvtNotAck;
    evt_blecore_aci *aci = (evt_blecore_aci *)hci->data;
    if (aci->ecode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) {
        aci_gatt_attribute_modified_event_rp0 *e = (void *)aci->data;
        if (e->Attr_Handle == tx_handle + 2 && e->Connection_Handle == connection) {
            bool enabled = e->Offset == 0 && e->Attr_Data_Length == 2 &&
                           e->Attr_Data[0] == 1 && e->Attr_Data[1] == 0;
            if (subscribed != enabled) ++generation; // invalidate pending work on unsubscribe
            subscribed = enabled;
            return SVCCTL_EvtAckFlowEnable;
        }
    } else if (aci->ecode == ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE) {
        aci_gatt_write_permit_req_event_rp0 *e = (void *)aci->data;
        if (e->Attribute_Handle == rx_handle + 1) {
            uint8_t error = 0;
            if (e->Connection_Handle != connection || !subscribed) error = 0x80;
            else if (!e->Data_Length || e->Data_Length > CONTROL_PACKET_MAX) error = 0x82;
            else if (!ControlApp_Enqueue(generation, e->Data, e->Data_Length)) error = 0x81;
            check(aci_gatt_permit_write(e->Connection_Handle, e->Attribute_Handle,
                                       error ? 1 : 0, error, e->Data_Length, e->Data));
            return SVCCTL_EvtAckFlowEnable;
        }
    }
    if (aci->ecode == ACI_GATT_PREPARE_WRITE_PERMIT_REQ_VSEVT_CODE) {
        aci_gatt_prepare_write_permit_req_event_rp0 *e = (void *)aci->data;
        if (e->Attribute_Handle == rx_handle + 1) {
            check(aci_gatt_permit_write(e->Connection_Handle, e->Attribute_Handle,
                                       1, 0x83, e->Data_Length, e->Data));
            return SVCCTL_EvtAckFlowEnable;
        }
    }
    return SVCCTL_EvtNotAck;
}
void ControlService_Init(void)
{
    if (SHCI_C2_SetFlashActivityControl(FLASH_ACTIVITY_CONTROL_SEM7) != SHCI_Success) Error_Handler();
    Service_UUID_t service_uuid;
    Char_UUID_t char_uuid;
    memcpy(&service_uuid, ControlService_UUID, 16);
    check(aci_gatt_add_service(UUID_TYPE_128, &service_uuid, PRIMARY_SERVICE, 6, &service_handle));
    memcpy(&char_uuid, ControlService_UUID, 16);
    ((uint8_t *)&char_uuid)[0] = 0x1b;
    check(aci_gatt_add_char(service_handle, UUID_TYPE_128, &char_uuid, CONTROL_PACKET_MAX,
          CHAR_PROP_WRITE, ATTR_PERMISSION_NONE, GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP,
          16, 1, &rx_handle));
    ((uint8_t *)&char_uuid)[0] = 0x1c;
    check(aci_gatt_add_char(service_handle, UUID_TYPE_128, &char_uuid, CONTROL_PACKET_MAX,
          CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 1, &tx_handle));
    SVCCTL_RegisterSvcHandler(event_handler);
}
int ControlService_Send(uint32_t session, const uint8_t *data, uint8_t length)
{
    if (!ControlService_IsCurrent(session)) return -1;
    tBleStatus status = aci_gatt_update_char_value_ext(connection, service_handle, tx_handle,
                                                     1, length, 0, length, data);
    if (status == BLE_STATUS_SUCCESS) return 0;
    if (!ControlService_IsCurrent(session)) return -1;
    if (status == BLE_STATUS_INSUFFICIENT_RESOURCES || status == BLE_STATUS_BUSY) return 1;
    // A permanent error cannot silently lose a reply: force client to reconnect.
    (void)hci_disconnect(connection, 0x13);
    return -1;
}
