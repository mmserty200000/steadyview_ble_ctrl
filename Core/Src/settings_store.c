#include "settings_store.h"
#include <string.h>
#define RECORD_MAGIC 0x53564346U
#define RECORD_VERSION 1U
#define RECORD_COMMIT UINT64_C(0x31444d4343565353)
typedef struct {
    uint32_t magic, version, revision, length;
    ReceiverSettings settings;
    uint8_t reserved[32];
    uint32_t crc, padding;
    uint64_t commit;
} Record;
_Static_assert(sizeof(Record) == SETTINGS_RECORD_SIZE, "Flash record layout changed");
_Static_assert(sizeof(ReceiverSettings) == 64, "Settings layout changed");
static uint32_t crc32(const void *data, size_t n)
{
    const uint8_t *p=data; uint32_t crc=0xffffffffU;
    while(n--) { crc^=*p++; for(unsigned i=0;i<8;++i) crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U); }
    return ~crc;
}
static bool valid(const Record *r)
{
    return r->magic==RECORD_MAGIC && r->version==RECORD_VERSION &&
           r->length==sizeof(ReceiverSettings) && r->commit==RECORD_COMMIT &&
           r->crc==crc32(r,offsetof(Record,crc));
}
static bool latest(Record *out, unsigned *page_out)
{
    bool found=false; Record r;
    for(unsigned page=0;page<2;++page)
        for(unsigned offset=0;offset<SETTINGS_PAGE_SIZE;offset+=sizeof(r)) {
            if (!SettingsFlash_Read(page,offset,&r,sizeof(r))) continue;
            if(valid(&r) && (!found || (int32_t)(r.revision-out->revision)>0)) {
                *out=r;*page_out=page;found=true;
            }
        }
    return found;
}
bool SettingsStore_Load(ReceiverSettings *settings, uint32_t *revision)
{
    Record r;unsigned page;
    if(!latest(&r,&page)) return false;
    *settings=r.settings;*revision=r.revision;return true;
}
bool SettingsStore_SaveKey(const ReceiverSettings *settings, const uint8_t key[32], uint32_t *revision)
{
    Record previous, next, probe;unsigned page=0;
    bool found=latest(&previous,&page);
    if(found && memcmp(settings,&previous.settings,sizeof(*settings))==0 &&
       (!key || memcmp(key,previous.reserved,32)==0)) {
        *revision=previous.revision;return true;
    }
    unsigned offset;
    for(offset=0;offset<SETTINGS_PAGE_SIZE;offset+=sizeof(probe)) {
        if(!SettingsFlash_Read(page,offset,&probe,sizeof(probe))) return false;
        const uint8_t *bytes=(const uint8_t *)&probe;unsigned i;
        for(i=0;i<sizeof(probe) && bytes[i]==0xff;++i) {}
        if(i==sizeof(probe)) break;
    }
    if(offset==SETTINGS_PAGE_SIZE) {
        page=1-page;offset=0;
        // Previous valid page is not erased until the new page has a committed record.
        if(!SettingsFlash_Erase(page)) return false;
    }
    memset(&next,0xff,sizeof(next));
    next.magic=RECORD_MAGIC;next.version=RECORD_VERSION;
    next.revision=found ? previous.revision+1 : 1;
    next.length=sizeof(*settings);next.settings=*settings;
    if(key)memcpy(next.reserved,key,32);
    else if(found)memcpy(next.reserved,previous.reserved,32);
    next.crc=crc32(&next,offsetof(Record,crc));next.commit=RECORD_COMMIT;
    for(unsigned i=0;i<sizeof(next);i+=8) {
        uint64_t word;memcpy(&word,(const uint8_t *)&next+i,8);
        if(!SettingsFlash_Program(page,offset+i,word))return false;
    }
    if(!SettingsFlash_Read(page,offset,&probe,sizeof(probe)) || !valid(&probe) ||
       memcmp(&probe,&next,sizeof(next))!=0)return false;
    *revision=next.revision;return true;
}

bool SettingsStore_Save(const ReceiverSettings *settings, uint32_t *revision)
{ return SettingsStore_SaveKey(settings,NULL,revision); }
bool SettingsStore_LoadKey(uint8_t key[32])
{
    Record r; unsigned page;
    if(!latest(&r,&page))return false;
    memcpy(key,r.reserved,32);
    for(unsigned i=0;i<32;++i)if(key[i]!=0xff)return true;
    return false;
}
