#ifndef CONTROL_AUTH_H
#define CONTROL_AUTH_H
#include "steadyview.pb.h"
#include <stdbool.h>
#include <stdint.h>
void ControlAuth_Init(void);
void ControlAuth_Session(uint32_t session);
/* Consumes authentication requests or unwraps/verifies a protected command. */
bool ControlAuth_Handle(steadyview_Request *request, steadyview_Response *reply);
bool AuthPlatform_Random(uint8_t nonce[32]);
void AuthPlatform_Salt(uint8_t salt[16]);
void AuthPlatform_Failed(void);
#endif
