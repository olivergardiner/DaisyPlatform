#ifndef PERSPECTIVE_HARDWARE_PINS_H
#define PERSPECTIVE_HARDWARE_PINS_H

#define KNOB_1_PIN seed::A0
#define KNOB_2_PIN seed::A1
#define KNOB_3_PIN seed::A2
#define KNOB_4_PIN seed::A3
#define KNOB_5_PIN seed::A4
#define KNOB_6_PIN seed::A5
#define KNOB_EXP_PIN seed::A6

#define SWITCH_1_PIN seed::D27
#define SWITCH_2_PIN seed::D28
#define SWITCH_3_PIN seed::D29
#define SWITCH_4_PIN seed::D30

#define ENCODER_1_A_PIN seed::D2
#define ENCODER_1_B_PIN seed::D3
#define ENCODER_1_BUTTON_PIN seed::D1
#define ENCODER_2_A_PIN seed::D5
#define ENCODER_2_B_PIN seed::D6
#define ENCODER_2_BUTTON_PIN seed::D4

#define LED_1_PIN seed::D25
#define LED_2_PIN seed::D26

#define AUDIO_IN_L_PIN seed::D13
#define AUDIO_IN_R_PIN seed::D14
#define AUDIO_OUT_L_PIN seed::D24
#define AUDIO_OUT_R_PIN seed::D23
#define EXPRESSION_PEDAL_PIN seed::D22
#define TRUE_BYPASS_PIN seed::D0

// Not strictly used as they are defined in the GFX2 UserConfig.h file
#define DISPLAY_DC_PIN seed::D12
#define DISPLAY_RESET_PIN seed::D11

#endif // PERSPECTIVE_HARDWARE_PINS_H