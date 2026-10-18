#include "control_protocol.h"
#include "control_auth.h"
#include "steadyview.h"
#include "pb_decode.h"
#include "pb_encode.h"
_Static_assert(steadyview_Request_size <= CONTROL_REQUEST_MAX, "Request buffer too small");
_Static_assert(steadyview_Response_size <= CONTROL_RESPONSE_MAX, "Response buffer too small");
static steadyview_SettingsInfo settings_info;
static steadyview_Status property_action(SteadyViewProperty selector,
    bool is_set, bool present, uint32_t *value)
{
    if (is_set != present) return steadyview_Status_MALFORMED_PACKET;
    return is_set ? SteadyView_Set(selector, *value) : SteadyView_Get(selector, value);
}
static steadyview_Status dispatch(steadyview_Property *property, bool is_set)
{
    switch (property->which_kind) {
    case steadyview_Property_channel_index_tag: {
        uint32_t value = property->kind.channel_index.value;
        steadyview_Status status = property_action(steadyview_Property_CHANNEL_INDEX, is_set,
            property->kind.channel_index.has_value, &value);
        property->kind.channel_index.has_value = status == steadyview_Status_OK;
        property->kind.channel_index.value = value;
        return status;
    }
    case steadyview_Property_video_mode_tag: {
        uint32_t value = property->kind.video_mode.value;
        steadyview_Status status = property_action(steadyview_Property_VIDEO_MODE, is_set,
            property->kind.video_mode.has_value, &value);
        property->kind.video_mode.has_value = status == steadyview_Status_OK;
        property->kind.video_mode.value = value;
        return status;
    }
    case steadyview_Property_frequency_mhz_tag: {
        uint32_t value = property->kind.frequency_mhz.value;
        steadyview_Status status = property_action(steadyview_Property_FREQUENCY_MHZ, is_set,
            property->kind.frequency_mhz.has_value, &value);
        property->kind.frequency_mhz.has_value = status == steadyview_Status_OK;
        property->kind.frequency_mhz.value = value;
        return status;
    }
    case steadyview_Property_initialize_tag: {
        uint32_t value = property->kind.initialize.value;
        steadyview_Status status = property_action(steadyview_Property_INITIALIZE, is_set,
            property->kind.initialize.has_value, &value);
        property->kind.initialize.has_value = status == steadyview_Status_OK;
        property->kind.initialize.value = value;
        return status;
    }
    case steadyview_Property_synthesizer_reg_a_tag: {
        uint32_t value = property->kind.synthesizer_reg_a.value;
        steadyview_Status status = property_action(steadyview_Property_SYNTHESIZER_REG_A, is_set,
            property->kind.synthesizer_reg_a.has_value, &value);
        property->kind.synthesizer_reg_a.has_value = status == steadyview_Status_OK;
        property->kind.synthesizer_reg_a.value = value;
        return status;
    }
    case steadyview_Property_synthesizer_reg_b_tag: {
        uint32_t value = property->kind.synthesizer_reg_b.value;
        steadyview_Status status = property_action(steadyview_Property_SYNTHESIZER_REG_B, is_set,
            property->kind.synthesizer_reg_b.has_value, &value);
        property->kind.synthesizer_reg_b.has_value = status == steadyview_Status_OK;
        property->kind.synthesizer_reg_b.value = value;
        return status;
    }
    case steadyview_Property_synthesizer_reg_c_tag: {
        uint32_t value = property->kind.synthesizer_reg_c.value;
        steadyview_Status status = property_action(steadyview_Property_SYNTHESIZER_REG_C, is_set,
            property->kind.synthesizer_reg_c.has_value, &value);
        property->kind.synthesizer_reg_c.has_value = status == steadyview_Status_OK;
        property->kind.synthesizer_reg_c.value = value;
        return status;
    }
    case steadyview_Property_synthesizer_reg_d_tag: {
        uint32_t value = property->kind.synthesizer_reg_d.value;
        steadyview_Status status = property_action(steadyview_Property_SYNTHESIZER_REG_D, is_set,
            property->kind.synthesizer_reg_d.has_value, &value);
        property->kind.synthesizer_reg_d.has_value = status == steadyview_Status_OK;
        property->kind.synthesizer_reg_d.value = value;
        return status;
    }
    case steadyview_Property_vco_switch_cap_control_tag: {
        uint32_t value = property->kind.vco_switch_cap_control.value;
        steadyview_Status status = property_action(steadyview_Property_VCO_SWITCH_CAP_CONTROL_REGISTER, is_set,
            property->kind.vco_switch_cap_control.has_value, &value);
        property->kind.vco_switch_cap_control.has_value = status == steadyview_Status_OK;
        property->kind.vco_switch_cap_control.value = value;
        return status;
    }
    case steadyview_Property_dfc_control_tag: {
        uint32_t value = property->kind.dfc_control.value;
        steadyview_Status status = property_action(steadyview_Property_DFC_CONTROL_REGISTER, is_set,
            property->kind.dfc_control.has_value, &value);
        property->kind.dfc_control.has_value = status == steadyview_Status_OK;
        property->kind.dfc_control.value = value;
        return status;
    }
    case steadyview_Property_six_m_audio_demodulator_control_tag: {
        uint32_t value = property->kind.six_m_audio_demodulator_control.value;
        steadyview_Status status = property_action(steadyview_Property_SIXM_AUDIO_DEMODULATOR_CONTROL_REGISTER, is_set,
            property->kind.six_m_audio_demodulator_control.has_value, &value);
        property->kind.six_m_audio_demodulator_control.has_value = status == steadyview_Status_OK;
        property->kind.six_m_audio_demodulator_control.value = value;
        return status;
    }
    case steadyview_Property_six_m5_audio_demodulator_control_tag: {
        uint32_t value = property->kind.six_m5_audio_demodulator_control.value;
        steadyview_Status status = property_action(steadyview_Property_SIXM5_AUDIO_DEMODULATOR_CONTROL_REGISTER, is_set,
            property->kind.six_m5_audio_demodulator_control.has_value, &value);
        property->kind.six_m5_audio_demodulator_control.has_value = status == steadyview_Status_OK;
        property->kind.six_m5_audio_demodulator_control.value = value;
        return status;
    }
    case steadyview_Property_receiver_control_1_tag: {
        uint32_t value = property->kind.receiver_control_1.value;
        steadyview_Status status = property_action(steadyview_Property_RECEIVER_CONTROL_REGISTER_1, is_set,
            property->kind.receiver_control_1.has_value, &value);
        property->kind.receiver_control_1.has_value = status == steadyview_Status_OK;
        property->kind.receiver_control_1.value = value;
        return status;
    }
    case steadyview_Property_receiver_control_2_tag: {
        uint32_t value = property->kind.receiver_control_2.value;
        steadyview_Status status = property_action(steadyview_Property_RECEIVER_CONTROL_REGISTER_2, is_set,
            property->kind.receiver_control_2.has_value, &value);
        property->kind.receiver_control_2.has_value = status == steadyview_Status_OK;
        property->kind.receiver_control_2.value = value;
        return status;
    }
    case steadyview_Property_power_down_control_tag: {
        uint32_t value = property->kind.power_down_control.value;
        steadyview_Status status = property_action(steadyview_Property_POWER_DOWN_CONTROL_REGISTER, is_set,
            property->kind.power_down_control.has_value, &value);
        property->kind.power_down_control.has_value = status == steadyview_Status_OK;
        property->kind.power_down_control.value = value;
        return status;
    }
    case steadyview_Property_state_register_tag: {
        uint32_t value = property->kind.state_register.value;
        steadyview_Status status = property_action(steadyview_Property_STATE_REGISTER, is_set,
            property->kind.state_register.has_value, &value);
        property->kind.state_register.has_value = status == steadyview_Status_OK;
        property->kind.state_register.value = value;
        return status;
    }
    default: return steadyview_Status_UNKNOWN_PROPERTY;
    }
}

void ControlProtocol_Init(void)
{
    ControlAuth_Init();
    ReceiverSettings settings;
    settings_info = (steadyview_SettingsInfo)steadyview_SettingsInfo_init_zero;
    if (SettingsStore_Load(&settings,&settings_info.revision)) {
        settings_info.saved = true;
        settings_info.restore_status = SteadyView_Restore(&settings);
        settings_info.dirty = settings_info.restore_status != steadyview_Status_OK;
        if (settings_info.dirty) (void)SteadyView_Init();
    } else {
        settings_info.restore_status = steadyview_Status_NO_SAVED_SETTINGS;
        settings_info.dirty = true;
        (void)SteadyView_Init();
    }
}
static steadyview_Status save_settings(void)
{
    ReceiverSettings settings;
    steadyview_Status status=SteadyView_Export(&settings);
    if(status!=steadyview_Status_OK)return status;
    if(!SettingsStore_Save(&settings,&settings_info.revision))return steadyview_Status_STORAGE_ERROR;
    settings_info.saved=true;settings_info.dirty=false;
    return steadyview_Status_OK;
}
static void get_state(steadyview_ReceiverState *state)
{
    const pb_size_t kinds[] = {1,2,3,4,16,17,18,19,20,21,22,23,24,25,26,31};
    state->properties_count=16;
    for(unsigned i=0;i<16;++i) {
        steadyview_PropertyResult *result=&state->properties[i];
        *result=(steadyview_PropertyResult)steadyview_PropertyResult_init_zero;
        result->has_property=true;
        result->property.which_kind=kinds[i];
        result->status=dispatch(&result->property,false);
    }
    state->has_settings=true;state->settings=settings_info;
}
bool ControlProtocol_Process(const uint8_t *data, size_t length,
                             uint8_t *response, size_t *response_length)
{
    steadyview_Request request = steadyview_Request_init_zero;
    steadyview_Response reply = steadyview_Response_init_zero;
    reply.status = steadyview_Status_MALFORMED_PACKET;
    pb_istream_t input = pb_istream_from_buffer(data, length);
    if (length && length <= CONTROL_REQUEST_MAX && pb_decode(&input, steadyview_Request_fields, &request)) {
        reply.request_id = request.request_id;
        if(ControlAuth_Handle(&request,&reply)) {
            if(request.which_command==steadyview_Request_set_passphrase_tag && reply.status==steadyview_Status_OK) {
                ReceiverSettings saved;
                settings_info.saved=SettingsStore_Load(&saved,&settings_info.revision);
                settings_info.dirty=false;
            }
            goto encode;
        }
        bool is_set = request.which_command == steadyview_Request_set_prop_tag;
        if (is_set || request.which_command == steadyview_Request_get_prop_tag) {
            reply.which_result=steadyview_Response_property_tag;
            steadyview_PropertyResult *result=&reply.result.property;
            result->has_property=is_set ? request.command.set_prop.has_property : request.command.get_prop.has_property;
            result->property=is_set ? request.command.set_prop.property : request.command.get_prop.property;
            result->status=steadyview_Status_MALFORMED_PACKET;
            if(result->has_property) result->status=dispatch(&result->property,is_set);
            reply.status=result->status;
            if(is_set && result->status==steadyview_Status_OK) {
                settings_info.dirty=true;
                reply.status=save_settings(); // PropertyResult OK means applied; top-level error can mean not persisted.
            } else if (is_set && result->status==steadyview_Status_IO_ERROR) settings_info.dirty=true;
        } else if(request.which_command==steadyview_Request_get_state_tag) {
            reply.status=steadyview_Status_OK;
            reply.which_result=steadyview_Response_state_tag;
            get_state(&reply.result.state);
        } else if(request.which_command==steadyview_Request_save_settings_tag ||
                  request.which_command==steadyview_Request_get_settings_info_tag) {
            reply.status=request.which_command==steadyview_Request_save_settings_tag ? save_settings() : steadyview_Status_OK;
            reply.which_result=steadyview_Response_settings_tag;
            reply.result.settings=settings_info;
        }
    }
encode:;
    pb_ostream_t output = pb_ostream_from_buffer(response, CONTROL_RESPONSE_MAX);
    if (!pb_encode(&output, steadyview_Response_fields, &reply)) return false;
    *response_length = output.bytes_written;
    return true;
}

void ControlProtocol_Session(uint32_t session){ControlAuth_Session(session);}
