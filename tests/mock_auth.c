#include "control_auth.h"
#include <string.h>
bool AuthPlatform_Random(uint8_t nonce[32]) {static uint8_t n;memset(nonce,++n,32);return true;}
void AuthPlatform_Salt(uint8_t salt[16]) {memset(salt,0x42,16);}
void AuthPlatform_Failed(void) {}
