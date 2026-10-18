#include "mock_flash.h"
#include <string.h>
static uint8_t flash[2][SETTINGS_PAGE_SIZE];
unsigned mock_flash_writes,mock_flash_erases;
int mock_flash_fail_after=-1;
void mock_flash_reset(void) { memset(flash,0xff,sizeof(flash));mock_flash_writes=mock_flash_erases=0;mock_flash_fail_after=-1; }
bool SettingsFlash_Read(unsigned p,unsigned o,void *d,size_t n) {
    if(p>=2 || o+n>SETTINGS_PAGE_SIZE)return false;
    memcpy(d,flash[p]+o,n);return true;
}
bool SettingsFlash_Erase(unsigned p) {
    if(p>=2 || mock_flash_fail_after==0)return false;
    if(mock_flash_fail_after>0)--mock_flash_fail_after;
    memset(flash[p],0xff,SETTINGS_PAGE_SIZE);++mock_flash_erases;return true;
}
bool SettingsFlash_Program(unsigned p,unsigned o,uint64_t value) {
    if(p>=2 || o%8 || o+8>SETTINGS_PAGE_SIZE || mock_flash_fail_after==0)return false;
    if(mock_flash_fail_after>0)--mock_flash_fail_after;
    uint8_t bytes[8];memcpy(bytes,&value,8);
    for(unsigned i=0;i<8;++i)if(flash[p][o+i]!=0xff)return false;
    memcpy(flash[p]+o,bytes,8);++mock_flash_writes;return true;
}

void mock_flash_corrupt(unsigned page,unsigned offset) { flash[page][offset]^=1; }
