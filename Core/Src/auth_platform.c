#include "control_auth.h"
#include "main.h"
#include "ble.h"
#include "cmsis_os2.h"
#include <string.h>
bool AuthPlatform_Random(uint8_t nonce[32])
{
    // Ask CPU2 to arbitrate its hardware RNG rather than racing it from CPU1.
    for(unsigned i=0;i<32;i+=8)if(hci_le_rand(nonce+i)!=BLE_STATUS_SUCCESS)return false;
    return true;
}
void AuthPlatform_Salt(uint8_t salt[16])
{
    const uint32_t words[4]={HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2(),0x31305653};
    memcpy(salt,words,16);
}
void AuthPlatform_Failed(void){osDelay(1000);}
