#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "steadyview.h"
#include "steadyview_bus.h"
#include "control_protocol.h"
#include "mock_flash.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "tinycrypt/hmac.h"
static uint32_t registers[16], writes, pulses;
static bool fail;
bool SVBus_Write(uint8_t address,uint32_t value) { assert(address<16 && value<=0xfffff);if(fail)return false;registers[address]=value;++writes;return true; }
bool SVBus_Read(uint8_t address,uint32_t *value) { if(fail)return false;*value=registers[address];return true; }
bool SVBus_MixPulse(void) { ++pulses;return !fail; }
void SVBus_DelayMs(uint32_t ms) { (void)ms; }
void SVBus_DelayUs(uint32_t us) { (void)us; }
static size_t response_length;
static steadyview_Response exchange(steadyview_Request request) {
    uint8_t input[CONTROL_REQUEST_MAX],output[CONTROL_RESPONSE_MAX];
    pb_ostream_t out=pb_ostream_from_buffer(input,sizeof(input));
    assert(pb_encode(&out,steadyview_Request_fields,&request));
    assert(ControlProtocol_Process(input,out.bytes_written,output,&response_length));
    steadyview_Response reply=steadyview_Response_init_zero;
    pb_istream_t in=pb_istream_from_buffer(output,response_length);
    assert(pb_decode(&in,steadyview_Response_fields,&reply));
    assert(reply.request_id==request.request_id);return reply;
}
static void sign_bytes(const uint8_t key[32],const char *domain,const uint8_t nonce[32],
                       const uint8_t *data,size_t n,uint32_t seq,uint8_t out[32]) {
    struct tc_hmac_state_struct h;
    uint8_t le[4]={(uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)(seq>>16),(uint8_t)(seq>>24)};
    assert(tc_hmac_set_key(&h,key,32) && tc_hmac_init(&h));
    assert(tc_hmac_update(&h,domain,strlen(domain)) && tc_hmac_update(&h,nonce,32));
    if(data)assert(tc_hmac_update(&h,le,4) && tc_hmac_update(&h,data,n));
    assert(tc_hmac_final(out,32,&h));
}
static steadyview_Request protect(steadyview_Request inner,const uint8_t key[32],const uint8_t nonce[32],uint32_t seq) {
    steadyview_Request req=steadyview_Request_init_zero;req.request_id=inner.request_id;
    req.which_command=steadyview_Request_secure_command_tag;
    steadyview_SecureCommand *cmd=&req.command.secure_command;
    pb_ostream_t out=pb_ostream_from_buffer(cmd->payload.bytes,64);
    assert(pb_encode(&out,steadyview_Request_fields,&inner));cmd->payload.size=out.bytes_written;
    cmd->sequence=seq;cmd->mac.size=32;
    sign_bytes(key,"SV-CMD-v1",nonce,cmd->payload.bytes,cmd->payload.size,seq,cmd->mac.bytes);
    return req;
}
static steadyview_Request authenticate(const uint8_t key[32],uint8_t nonce[32]) {
    steadyview_Request req=steadyview_Request_init_zero;req.which_command=steadyview_Request_get_auth_tag;
    steadyview_Response reply=exchange(req);assert(reply.status==steadyview_Status_OK && reply.result.auth.challenge.size==32);
    memcpy(nonce,reply.result.auth.challenge.bytes,32);
    req.which_command=steadyview_Request_authenticate_tag;req.command.authenticate.proof.size=32;
    sign_bytes(key,"SV-AUTH-v1",nonce,NULL,0,0,req.command.authenticate.proof.bytes);return req;
}
static void auth_tests(void) {
    // RFC 4231 test case 1, independently specified HMAC-SHA256 expected output.
    struct tc_hmac_state_struct h;uint8_t rfc_key[20],digest[32];memset(rfc_key,0x0b,20);
    const uint8_t expected[32]={0xb0,0x34,0x4c,0x61,0xd8,0xdb,0x38,0x53,0x5c,0xa8,0xaf,0xce,0xaf,0x0b,0xf1,0x2b,0x88,0x1d,0xc2,0x00,0xc9,0x83,0x3d,0xa7,0x26,0xe9,0x37,0x6c,0x2e,0x32,0xcf,0xf7};
    assert(tc_hmac_set_key(&h,rfc_key,20) && tc_hmac_init(&h) && tc_hmac_update(&h,"Hi There",8) && tc_hmac_final(digest,32,&h));
    assert(memcmp(digest,expected,32)==0);
    fail=false;mock_flash_reset();ControlProtocol_Init();ControlProtocol_Session(10);
    uint8_t key[32],next[32],nonce[32],loaded[32];memset(key,0x42,32);memset(next,0x24,32);
    steadyview_Request req=steadyview_Request_init_zero;
    req.which_command=steadyview_Request_set_passphrase_tag;req.command.set_passphrase.key.size=32;
    memcpy(req.command.set_passphrase.key.bytes,key,32);
    mock_flash_fail_after=0;assert(exchange(req).status==steadyview_Status_STORAGE_ERROR);
    assert(!SettingsStore_LoadKey(loaded));mock_flash_fail_after=-1;
    assert(exchange(req).status==steadyview_Status_OK);assert(SettingsStore_LoadKey(loaded) && !memcmp(loaded,key,32));
    assert(exchange(req).status==steadyview_Status_AUTH_REQUIRED); // cannot overwrite provisioned key
    req.which_command=steadyview_Request_get_state_tag;
    assert(exchange(req).status==steadyview_Status_AUTH_REQUIRED);
    steadyview_Request proof=authenticate(next,nonce);assert(exchange(proof).status==steadyview_Status_AUTH_FAILED);
    proof=authenticate(key,nonce);assert(exchange(proof).status==steadyview_Status_OK);
    assert(exchange(req).status==steadyview_Status_AUTH_REQUIRED); // bare commands blocked even after login
    steadyview_Request secured=protect(req,key,nonce,1),bad=secured;bad.command.secure_command.mac.bytes[0]^=1;
    assert(exchange(bad).status==steadyview_Status_AUTH_FAILED);
    assert(exchange(secured).status==steadyview_Status_OK);
    assert(exchange(secured).status==steadyview_Status_AUTH_FAILED); // replay
    assert(exchange(proof).status==steadyview_Status_AUTH_FAILED); // replaying proof must not reset sequence
    assert(exchange(protect(req,key,nonce,2)).status==steadyview_Status_AUTH_REQUIRED);
    proof=authenticate(key,nonce);assert(exchange(proof).status==steadyview_Status_OK);
    ControlProtocol_Session(11);assert(exchange(protect(req,key,nonce,1)).status==steadyview_Status_AUTH_REQUIRED);
    assert(exchange(proof).status==steadyview_Status_AUTH_FAILED);
    // Normal settings saves and restart preserve authentication.
    proof=authenticate(key,nonce);assert(exchange(proof).status==steadyview_Status_OK);
    req.which_command=steadyview_Request_save_settings_tag;
    assert(exchange(protect(req,key,nonce,1)).status==steadyview_Status_OK);
    ControlProtocol_Init();assert(exchange(req).status==steadyview_Status_AUTH_REQUIRED);
    proof=authenticate(key,nonce);assert(exchange(proof).status==steadyview_Status_OK);
    memset(&req.command,0,sizeof(req.command));req.which_command=steadyview_Request_set_passphrase_tag;
    req.command.set_passphrase.key.size=32;sign_bytes(key,"SV-KEY-v1",nonce,NULL,0,0,digest);
    for(unsigned i=0;i<32;++i)req.command.set_passphrase.key.bytes[i]=next[i]^digest[i];
    mock_flash_fail_after=0;assert(exchange(protect(req,key,nonce,1)).status==steadyview_Status_STORAGE_ERROR);
    assert(SettingsStore_LoadKey(loaded) && !memcmp(loaded,key,32));mock_flash_fail_after=-1;
    assert(exchange(protect(req,key,nonce,2)).status==steadyview_Status_OK);
    assert(SettingsStore_LoadKey(loaded) && !memcmp(loaded,next,32));
    proof=authenticate(key,nonce);assert(exchange(proof).status==steadyview_Status_AUTH_FAILED);
    proof=authenticate(next,nonce);assert(exchange(proof).status==steadyview_Status_OK);
    puts("auth: provisioning, replay, session, rotation, persistence and RFC 4231 passed");
}

int main(void) {
    mock_flash_reset();ControlProtocol_Init();
    assert(registers[0]==8 && pulses==1 && writes==3);
    const uint32_t frequencies[]={5865,5845,5825,5805,5785,5765,5745,5725,
        5733,5752,5771,5790,5809,5828,5847,5866,5705,5685,5665,5645,5885,5905,5925,5945,
        5740,5760,5780,5800,5820,5840,5860,5880,5658,5695,5732,5769,5806,5843,5880,5917,
        5333,5373,5413,5453,5493,5533,5573,5613};
    for(unsigned i=0;i<48;++i) {
        assert(SteadyView_Set(steadyview_Property_CHANNEL_INDEX,i)==steadyview_Status_OK);
        uint32_t f=(frequencies[i]-479)/2,value;
        assert(registers[1]==((f/32)<<7 | f%32));
        assert(SteadyView_Get(steadyview_Property_FREQUENCY_MHZ,&value)==steadyview_Status_OK && value==frequencies[i]);
    }
    unsigned old=writes;
    assert(SteadyView_Set(steadyview_Property_CHANNEL_INDEX,47)==steadyview_Status_OK && writes==old);
    assert(SteadyView_Set(steadyview_Property_CHANNEL_INDEX,48)==steadyview_Status_INVALID_VALUE);
    assert(SteadyView_Set(steadyview_Property_VIDEO_MODE,2)==steadyview_Status_INVALID_VALUE);
    assert(SteadyView_Set(steadyview_Property_STATE_REGISTER,0)==steadyview_Status_READ_ONLY);
    steadyview_Request req=steadyview_Request_init_zero;
    req.request_id=0xffffffff;req.which_command=steadyview_Request_set_prop_tag;
    req.command.set_prop.has_property=true;
    req.command.set_prop.property.which_kind=steadyview_Property_channel_index_tag;
    req.command.set_prop.property.kind.channel_index.has_value=true;
    req.command.set_prop.property.kind.channel_index.value=0;
    steadyview_Response reply=exchange(req);
    assert(reply.status==steadyview_Status_OK && reply.result.property.property.kind.channel_index.has_value);
    assert(mock_flash_writes==16);
    assert(exchange(req).status==steadyview_Status_OK && mock_flash_writes==16); // no wear for unchanged settings
    req.command.set_prop.property.kind.channel_index.has_value=false;
    assert(exchange(req).status==steadyview_Status_MALFORMED_PACKET);
    req.which_command=steadyview_Request_get_prop_tag;
    assert(exchange(req).status==steadyview_Status_OK);
    req.which_command=steadyview_Request_get_state_tag;
    reply=exchange(req);
    assert(reply.status==steadyview_Status_OK && reply.result.state.properties_count==16);
    assert(response_length>20 && response_length<=CONTROL_RESPONSE_MAX);
    assert(reply.result.state.settings.saved && !reply.result.state.settings.dirty);
    // All writable register variants dispatch to distinct register addresses.
#define CHECK_REGISTER(field,address) do { \
    memset(&req.command,0,sizeof(req.command));req.which_command=steadyview_Request_set_prop_tag; \
    req.command.set_prop.has_property=true;req.command.set_prop.property.which_kind=steadyview_Property_##field##_tag; \
    req.command.set_prop.property.kind.field.has_value=true;req.command.set_prop.property.kind.field.value=0xabcde; \
    assert(exchange(req).status==steadyview_Status_OK && registers[address]==0xabcde); \
} while(0)
    CHECK_REGISTER(synthesizer_reg_a,0);CHECK_REGISTER(synthesizer_reg_b,1);
    CHECK_REGISTER(synthesizer_reg_c,2);CHECK_REGISTER(synthesizer_reg_d,3);
    CHECK_REGISTER(vco_switch_cap_control,4);CHECK_REGISTER(dfc_control,5);
    CHECK_REGISTER(six_m_audio_demodulator_control,6);CHECK_REGISTER(six_m5_audio_demodulator_control,7);
    CHECK_REGISTER(receiver_control_1,8);CHECK_REGISTER(receiver_control_2,9);CHECK_REGISTER(power_down_control,10);
    uint32_t saved[11];memcpy(saved,registers,sizeof(saved));
    memset(registers,0,sizeof(registers));ControlProtocol_Init();
    assert(memcmp(saved,registers,sizeof(saved))==0); // restores every register, including raw overrides
    req.which_command=steadyview_Request_get_settings_info_tag;
    reply=exchange(req);assert(reply.result.settings.restore_status==steadyview_Status_OK);
    // Select a channel, preserve it across restart, then simulate storage failure.
    memset(&req.command,0,sizeof(req.command));req.which_command=steadyview_Request_set_prop_tag;
    req.command.set_prop.has_property=true;req.command.set_prop.property.which_kind=steadyview_Property_channel_index_tag;
    req.command.set_prop.property.kind.channel_index.has_value=true;req.command.set_prop.property.kind.channel_index.value=12;
    assert(exchange(req).status==steadyview_Status_OK);ControlProtocol_Init();
    uint32_t value;assert(SteadyView_Get(steadyview_Property_CHANNEL_INDEX,&value)==steadyview_Status_OK && value==12);
    req.command.set_prop.property.kind.channel_index.value=13;mock_flash_fail_after=0;
    reply=exchange(req);assert(reply.status==steadyview_Status_STORAGE_ERROR && reply.result.property.status==steadyview_Status_OK);
    req.which_command=steadyview_Request_get_settings_info_tag;
    reply=exchange(req);assert(reply.result.settings.dirty);
    mock_flash_fail_after=-1;req.which_command=steadyview_Request_save_settings_tag;
    assert(exchange(req).status==steadyview_Status_OK);
    uint8_t bad[]={0x12,0xff},output[CONTROL_RESPONSE_MAX];size_t n;
    assert(ControlProtocol_Process(bad,sizeof(bad),output,&n));
    pb_istream_t in=pb_istream_from_buffer(output,n);assert(pb_decode(&in,steadyview_Response_fields,&reply));
    assert(reply.status==steadyview_Status_MALFORMED_PACKET);
    fail=true;assert(SteadyView_Get(steadyview_Property_STATE_REGISTER,&value)==steadyview_Status_IO_ERROR);
    puts("request/response, state and restore: passed");
    auth_tests();
}
