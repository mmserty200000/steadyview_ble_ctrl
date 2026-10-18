#ifndef STEADYVIEW_H
#define STEADYVIEW_H
#include "steadyview.pb.h"
#include "settings_store.h"
typedef enum {
 steadyview_Property_PROPERTY_UNSPECIFIED = 0,
 steadyview_Property_CHANNEL_INDEX = 1,
 steadyview_Property_VIDEO_MODE = 2,
 steadyview_Property_FREQUENCY_MHZ = 3,
 steadyview_Property_INITIALIZE = 4,
 steadyview_Property_SYNTHESIZER_REG_A = 16,
 steadyview_Property_SYNTHESIZER_REG_B = 17,
 steadyview_Property_SYNTHESIZER_REG_C = 18,
 steadyview_Property_SYNTHESIZER_REG_D = 19,
 steadyview_Property_VCO_SWITCH_CAP_CONTROL_REGISTER = 20,
 steadyview_Property_DFC_CONTROL_REGISTER = 21,
 steadyview_Property_SIXM_AUDIO_DEMODULATOR_CONTROL_REGISTER = 22,
 steadyview_Property_SIXM5_AUDIO_DEMODULATOR_CONTROL_REGISTER = 23,
 steadyview_Property_RECEIVER_CONTROL_REGISTER_1 = 24,
 steadyview_Property_RECEIVER_CONTROL_REGISTER_2 = 25,
 steadyview_Property_POWER_DOWN_CONTROL_REGISTER = 26,
 steadyview_Property_STATE_REGISTER = 31,
} SteadyViewProperty;
steadyview_Status SteadyView_Init(void);
steadyview_Status SteadyView_Set(SteadyViewProperty property, uint32_t value);
steadyview_Status SteadyView_Get(SteadyViewProperty property, uint32_t *value);
steadyview_Status SteadyView_Export(ReceiverSettings *settings);
steadyview_Status SteadyView_Restore(const ReceiverSettings *settings);
#endif
