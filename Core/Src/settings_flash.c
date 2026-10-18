/* Flash arbitration follows ST BLE_RfWithFlash flash_driver.c (BSD-3-Clause).
 * Each erase/doubleword obtains SEM7, with SEM2 owning the Flash peripheral.
 */
#include "settings_store.h"
#include "app_common.h"
#include "main.h"
#include "shci.h"
#include "cmsis_os2.h"
#include "steadyview_bus.h"
#include <string.h>
extern uint8_t __settings_start__;
static bool bounds(unsigned page,unsigned offset,size_t size)
{
    return page<2 && offset<=SETTINGS_PAGE_SIZE && size<=SETTINGS_PAGE_SIZE-offset;
}
static uint32_t address(unsigned page,unsigned offset)
{
    return (uint32_t)&__settings_start__+page*SETTINGS_PAGE_SIZE+offset;
}
bool SettingsFlash_Read(unsigned page,unsigned offset,void *data,size_t size)
{
    if(!bounds(page,offset,size))return false;
    memcpy(data,(const void *)address(page,offset),size);return true;
}
static bool operate(unsigned page,unsigned offset,uint64_t data,bool erase)
{
    if(!bounds(page,offset,erase ? SETTINGS_PAGE_SIZE : 8) || offset%8)return false;
    if ((address(page,0)-FLASH_BASE)/FLASH_PAGE_SIZE >=
        ((FLASH->SFR & FLASH_SFR_SFSA) >> FLASH_SFR_SFSA_Pos)) return false;
    uint32_t start=HAL_GetTick();
    while(LL_HSEM_1StepLock(HSEM,CFG_HW_FLASH_SEMID)) {
        if(HAL_GetTick()-start>2000)return false;
        osDelay(1);
    }
    bool ok=false,erase_announced=false;
    if(HAL_FLASH_Unlock()!=HAL_OK)goto cleanup;
    if(erase) {
        if(SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_ON)!=SHCI_Success)goto cleanup;
        erase_announced=true;
        SVBus_DelayUs(5);
    }
    start=HAL_GetTick();
    while(HAL_GetTick()-start<2000) {
        if(LL_FLASH_IsActiveFlag_OperationSuspended()) { osDelay(1);continue; }
        uint32_t mask=__get_PRIMASK();__disable_irq();
        if(!LL_HSEM_GetStatus(HSEM,CFG_HW_BLOCK_FLASH_REQ_BY_CPU1_SEMID) &&
           !LL_HSEM_1StepLock(HSEM,CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID)) {
            if(erase) {
                FLASH_EraseInitTypeDef e={0};uint32_t error;
                e.TypeErase=FLASH_TYPEERASE_PAGES;e.NbPages=1;
                e.Page=(address(page,0)-FLASH_BASE)/FLASH_PAGE_SIZE;
                ok=HAL_FLASHEx_Erase(&e,&error)==HAL_OK;
            } else ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,address(page,offset),data)==HAL_OK;
            LL_HSEM_ReleaseLock(HSEM,CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID,0);
            __set_PRIMASK(mask);
            SVBus_DelayUs(1); // Give CPU2 an opportunity to acquire SEM7 before the next operation.
            break;
        }
        __set_PRIMASK(mask);osDelay(1);
    }
cleanup:
    if(erase_announced && SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_OFF)!=SHCI_Success)ok=false;
    HAL_FLASH_Lock();
    LL_HSEM_ReleaseLock(HSEM,CFG_HW_FLASH_SEMID,0);
    return ok;
}
bool SettingsFlash_Erase(unsigned page) { return operate(page,0,0,true); }
bool SettingsFlash_Program(unsigned page,unsigned offset,uint64_t data) { return operate(page,offset,data,false); }
