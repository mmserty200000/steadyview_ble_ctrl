/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from ExpressLRS/Backpack src/steadyview.{h,cpp}.
 * See LICENSES/Backpack-GPL-3.0.txt and Protocol/README.md.
 */
#include "steadyview.h"
#include "steadyview_bus.h"

static const uint16_t frequencies[48] = {
    5865,5845,5825,5805,5785,5765,5745,5725,
    5733,5752,5771,5790,5809,5828,5847,5866,
    5705,5685,5665,5645,5885,5905,5925,5945,
    5740,5760,5780,5800,5820,5840,5860,5880,
    5658,5695,5732,5769,5806,5843,5880,5917,
    5333,5373,5413,5453,5493,5533,5573,5613
};
static uint8_t channel;
static uint8_t mode;
static bool initialized, channel_known, mode_known;

static uint32_t synth(uint8_t index)
{
    uint32_t f = (frequencies[index] - 479U) / 2U;
    return ((f / 32U) << 7) | (f % 32U);
}
static bool register_property(SteadyViewProperty p)
{
    return (p >= steadyview_Property_SYNTHESIZER_REG_A &&
            p <= steadyview_Property_POWER_DOWN_CONTROL_REGISTER) ||
            p == steadyview_Property_STATE_REGISTER;
}
static steadyview_Status set_mode(uint32_t value)
{
    if (value > steadyview_VideoModeKind_DIVERSITY) return steadyview_Status_INVALID_VALUE;
    if (!channel_known) return steadyview_Status_NOT_INITIALIZED;
    mode_known = false;
    if (value == steadyview_VideoModeKind_MIX && !SVBus_MixPulse())
        return steadyview_Status_IO_ERROR;
    if (!SVBus_Write(0, 8)) return steadyview_Status_IO_ERROR;
    SVBus_DelayUs(500);
    if (!SVBus_Write(0, 8) || !SVBus_Write(1, synth(channel)))
        return steadyview_Status_IO_ERROR;
    mode = (uint8_t)value;
    mode_known = true;
    return steadyview_Status_OK;
}
steadyview_Status SteadyView_Init(void)
{
    initialized = false;
    channel = 0;
    channel_known = true;
    mode_known = false;
    SVBus_DelayMs(100);
    steadyview_Status status = set_mode(steadyview_VideoModeKind_MIX);
    initialized = (status == steadyview_Status_OK);
    if (!initialized) channel_known = false;
    return status;
}
steadyview_Status SteadyView_Set(SteadyViewProperty property, uint32_t value)
{
    if (property == steadyview_Property_INITIALIZE)
        return value == 1 ? SteadyView_Init() : steadyview_Status_INVALID_VALUE;
    if (property == steadyview_Property_FREQUENCY_MHZ || property == steadyview_Property_STATE_REGISTER)
        return steadyview_Status_READ_ONLY;
    if (register_property(property)) {
        if (value > 0xFFFFFU) return steadyview_Status_INVALID_VALUE;
        uint8_t address = (uint8_t)(property - 16);
        // Raw writes can invalidate the high-level channel/mode state.
        channel_known = false;
        mode_known = false;
        return SVBus_Write(address, value) ? steadyview_Status_OK : steadyview_Status_IO_ERROR;
    }
    if (property != steadyview_Property_CHANNEL_INDEX && property != steadyview_Property_VIDEO_MODE)
        return steadyview_Status_UNKNOWN_PROPERTY;
    if (!initialized) return steadyview_Status_NOT_INITIALIZED;
    if (property == steadyview_Property_VIDEO_MODE) return set_mode(value);
    if (value >= 48) return steadyview_Status_INVALID_VALUE;
    uint32_t actual;
    if (!SVBus_Read(1, &actual)) return steadyview_Status_IO_ERROR;
    if (actual != synth((uint8_t)value)) {
        channel_known = false;
        if (!SVBus_Write(0, 8) || !SVBus_Write(1, synth((uint8_t)value)))
            return steadyview_Status_IO_ERROR;
    }
    channel = (uint8_t)value;
    channel_known = true;
    return steadyview_Status_OK;
}
steadyview_Status SteadyView_Get(SteadyViewProperty property, uint32_t *value)
{
    if (register_property(property))
        return SVBus_Read((uint8_t)(property - 16), value) ? steadyview_Status_OK : steadyview_Status_IO_ERROR;
    switch (property) {
    case steadyview_Property_INITIALIZE: *value = initialized; return steadyview_Status_OK;
    case steadyview_Property_CHANNEL_INDEX:
    case steadyview_Property_FREQUENCY_MHZ:
        if (!initialized || !channel_known) return steadyview_Status_NOT_INITIALIZED;
        *value = property == steadyview_Property_CHANNEL_INDEX ? channel : frequencies[channel];
        return steadyview_Status_OK;
    case steadyview_Property_VIDEO_MODE:
        if (!initialized || !mode_known) return steadyview_Status_NOT_INITIALIZED;
        *value = mode; return steadyview_Status_OK;
    default: return steadyview_Status_UNKNOWN_PROPERTY;
    }
}

steadyview_Status SteadyView_Export(ReceiverSettings *settings)
{
    if (!initialized) return steadyview_Status_NOT_INITIALIZED;
    *settings = (ReceiverSettings){0};
    settings->channel = channel;
    settings->frequency_mhz = frequencies[channel];
    settings->mode = mode;
    settings->flags = (channel_known ? SETTINGS_CHANNEL_KNOWN : 0) |
                      (mode_known ? SETTINGS_MODE_KNOWN : 0);
    for (uint8_t address=0;address<11;++address) {
        if (!SVBus_Read(address, &settings->registers[address])) return steadyview_Status_IO_ERROR;
        settings->register_mask |= 1U << address;
    }
    return steadyview_Status_OK;
}
steadyview_Status SteadyView_Restore(const ReceiverSettings *settings)
{
    if (settings->channel >= 48 || settings->mode > 1 ||
        settings->frequency_mhz != frequencies[settings->channel] ||
        settings->register_mask != 0x7ff || (settings->flags & ~3U))
        return steadyview_Status_STORAGE_ERROR;
    for (unsigned i=0;i<11;++i)
        if (settings->registers[i]>0xfffff) return steadyview_Status_STORAGE_ERROR;
    steadyview_Status status = SteadyView_Init();
    if (status != steadyview_Status_OK) return status;
    channel = (uint8_t)settings->channel;
    status = set_mode(settings->mode);
    if (status != steadyview_Status_OK) { initialized=false; return status; }
    // Restore synthesizer and power-down last, after selecting receiver mode.
    const uint8_t order[] = {2,3,4,5,6,7,8,9,0,1,10};
    for (unsigned i=0;i<sizeof(order);++i)
        if (!SVBus_Write(order[i],settings->registers[order[i]])) {
            initialized=false;channel_known=false;mode_known=false;
            return steadyview_Status_IO_ERROR;
        }
    channel_known = (settings->flags & SETTINGS_CHANNEL_KNOWN) != 0;
    mode_known = (settings->flags & SETTINGS_MODE_KNOWN) != 0;
    return steadyview_Status_OK;
}
