#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mock_flash.h"
int main(void) {
    ReceiverSettings old={0},next={0},loaded;uint32_t revision;
    mock_flash_reset();assert(!SettingsStore_Load(&loaded,&revision));
    old.channel=3;assert(SettingsStore_Save(&old,&revision) && revision==1);
    unsigned writes=mock_flash_writes;
    assert(SettingsStore_Save(&old,&revision) && revision==1 && writes==mock_flash_writes);
    // Interrupt each doubleword of an append: previous complete record survives.
    for(int i=0;i<16;++i) {
        mock_flash_reset();assert(SettingsStore_Save(&old,&revision));
        next=old;next.channel=5;mock_flash_fail_after=i;
        assert(!SettingsStore_Save(&next,&revision));mock_flash_fail_after=-1;
        assert(SettingsStore_Load(&loaded,&revision) && revision==1 && loaded.channel==3);
        assert(SettingsStore_Save(&next,&revision) && revision==2);
    }
    // Two full rotations, including interrupted erase of the non-current page.
    mock_flash_reset();
    for(unsigned i=0;i<64;++i) { next.channel=i;assert(SettingsStore_Save(&next,&revision)); }
    assert(revision==64 && mock_flash_erases==1);
    mock_flash_fail_after=0;next.channel=64;
    assert(!SettingsStore_Save(&next,&revision));mock_flash_fail_after=-1;
    assert(SettingsStore_Load(&loaded,&revision) && revision==64 && loaded.channel==63);
    assert(SettingsStore_Save(&next,&revision) && revision==65 && mock_flash_erases==2);
    assert(SettingsStore_Load(&loaded,&revision) && memcmp(&next,&loaded,sizeof(next))==0);
    for(int cut=0;cut<17;++cut) {
        mock_flash_reset();
        for(unsigned i=0;i<32;++i){next.channel=i;assert(SettingsStore_Save(&next,&revision));}
        mock_flash_fail_after=cut;next.channel=32;
        assert(!SettingsStore_Save(&next,&revision));mock_flash_fail_after=-1;
        assert(SettingsStore_Load(&loaded,&revision) && revision==32 && loaded.channel==31);
    }
    mock_flash_reset();old.channel=1;next.channel=2;
    assert(SettingsStore_Save(&old,&revision));assert(SettingsStore_Save(&next,&revision));
    mock_flash_corrupt(0,SETTINGS_RECORD_SIZE+16); // corruption in payload with valid commit
    assert(SettingsStore_Load(&loaded,&revision) && revision==1 && loaded.channel==1);
    mock_flash_corrupt(0,16);
    assert(!SettingsStore_Load(&loaded,&revision));
    puts("settings journal: passed");
}
