#include "settings_store.h"
extern unsigned mock_flash_writes, mock_flash_erases;
extern int mock_flash_fail_after;
void mock_flash_reset(void);
void mock_flash_corrupt(unsigned page, unsigned offset);
