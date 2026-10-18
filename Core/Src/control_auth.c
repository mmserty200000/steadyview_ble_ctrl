#include "control_auth.h"
#include "steadyview.h"
#include "pb_decode.h"
#include "tinycrypt/hmac.h"
#include <string.h>
static uint8_t key[32], challenge[32];
static bool configured, authorized, challenge_valid, proof_used;
static uint32_t active_session, sequence;
static void reset_session(void)
{
    authorized=false;challenge_valid=false;proof_used=false;sequence=0;
    memset(challenge,0,sizeof(challenge));
}
void ControlAuth_Init(void)
{
    memset(key,0,sizeof(key));configured=SettingsStore_LoadKey(key);
    active_session=0;reset_session();
}
void ControlAuth_Session(uint32_t session)
{
    if(active_session!=session){active_session=session;reset_session();}
}
static bool mac(const char *domain, const uint8_t *payload, size_t size,
                uint32_t seq, uint8_t out[32])
{
    struct tc_hmac_state_struct h;
    uint8_t le[4]={(uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)(seq>>16),(uint8_t)(seq>>24)};
    bool ok=tc_hmac_set_key(&h,key,32) && tc_hmac_init(&h) &&
        tc_hmac_update(&h,domain,strlen(domain)) && tc_hmac_update(&h,challenge,32);
    if(payload)ok=ok && tc_hmac_update(&h,le,4) && tc_hmac_update(&h,payload,size);
    ok=ok && tc_hmac_final(out,32,&h);
    memset(&h,0,sizeof(h));return ok;
}
static bool equal(const uint8_t a[32],const uint8_t b[32])
{
    volatile uint8_t diff=0;
    for(unsigned i=0;i<32;++i)diff|=a[i]^b[i];
    return diff==0;
}
static void auth_state(steadyview_Response *reply)
{
    reply->which_result=steadyview_Response_auth_tag;
    steadyview_AuthState *state=&reply->result.auth;
    *state=(steadyview_AuthState)steadyview_AuthState_init_zero;
    state->configured=configured;state->authorized=authorized;
    state->salt.size=16;AuthPlatform_Salt(state->salt.bytes);
    state->iterations=600000;
    if(challenge_valid){state->challenge.size=32;memcpy(state->challenge.bytes,challenge,32);}
}
bool ControlAuth_Handle(steadyview_Request *request, steadyview_Response *reply)
{
    bool secure=false;
    uint8_t expected[32];
    if(request->which_command==steadyview_Request_secure_command_tag) {
        steadyview_SecureCommand *cmd=&request->command.secure_command;
        if(!configured || !authorized || !challenge_valid) {
            reply->status=steadyview_Status_AUTH_REQUIRED;return true;
        }
        if(sequence==UINT32_MAX || cmd->sequence!=sequence+1 || cmd->mac.size!=32 ||
           !mac("SV-CMD-v1",cmd->payload.bytes,cmd->payload.size,cmd->sequence,expected) ||
           !equal(expected,cmd->mac.bytes)) {
            reply->status=steadyview_Status_AUTH_FAILED;AuthPlatform_Failed();return true;
        }
        steadyview_Request inner=steadyview_Request_init_zero;
        pb_istream_t stream=pb_istream_from_buffer(cmd->payload.bytes,cmd->payload.size);
        ++sequence; // A valid MAC consumes the sequence even if the inner protobuf is invalid.
        if(!pb_decode(&stream,steadyview_Request_fields,&inner) || inner.request_id!=request->request_id ||
           inner.which_command<2 || inner.which_command>9 ||
           inner.which_command==steadyview_Request_get_auth_tag ||
           inner.which_command==steadyview_Request_authenticate_tag) {
            reply->status=steadyview_Status_MALFORMED_PACKET;return true;
        }
        *request=inner;secure=true;
    }
    if(request->which_command==steadyview_Request_get_auth_tag) {
        reset_session();
        challenge_valid=AuthPlatform_Random(challenge);
        reply->status=challenge_valid ? steadyview_Status_OK : steadyview_Status_IO_ERROR;
        auth_state(reply);return true;
    }
    if(request->which_command==steadyview_Request_authenticate_tag) {
        authorized=false;
        bool ok=configured && challenge_valid && !proof_used && request->command.authenticate.proof.size==32 &&
            mac("SV-AUTH-v1",NULL,0,0,expected) && equal(expected,request->command.authenticate.proof.bytes);
        proof_used=true;
        // Single-use proof, including a failed attempt; GetAuth issues a fresh challenge.
        if(ok){authorized=true;sequence=0;}
        else {challenge_valid=false;AuthPlatform_Failed();}
        reply->status=ok ? steadyview_Status_OK : steadyview_Status_AUTH_FAILED;
        auth_state(reply);return true;
    }
    if(configured && !secure){reply->status=steadyview_Status_AUTH_REQUIRED;return true;}
    if(request->which_command==steadyview_Request_set_passphrase_tag) {
        steadyview_SetPassphrase *set=&request->command.set_passphrase;
        reply->status=steadyview_Status_INVALID_VALUE;
        if(set->key.size!=32)return true;
        uint8_t next[32];memcpy(next,set->key.bytes,32);
        if(configured) {
            if(!mac("SV-KEY-v1",NULL,0,0,expected)){reply->status=steadyview_Status_IO_ERROR;return true;}
            for(unsigned i=0;i<32;++i)next[i]^=expected[i];
        }
        unsigned ff=0;for(unsigned i=0;i<32;++i)ff+=next[i]==0xff;
        if(ff==32)return true; // Journal sentinel cannot be a credential.
        ReceiverSettings settings;uint32_t revision;
        reply->status=SteadyView_Export(&settings);
        if(reply->status==steadyview_Status_OK) {
            if(!SettingsStore_SaveKey(&settings,next,&revision))reply->status=steadyview_Status_STORAGE_ERROR;
            else {memcpy(key,next,32);configured=true;reset_session();}
        }
        memset(next,0,sizeof(next));auth_state(reply);return true;
    }
    return false;
}
