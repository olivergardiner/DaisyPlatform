#pragma once

// Hardware control indices for parameter mapping
// These indices are used to map effect parameters to physical controls

namespace perspective {

// ========== KNOB INDICES ==========
// Analog potentiometers (ADC inputs)
#define KNOB_1_IDX      0
#define KNOB_2_IDX      1
#define KNOB_3_IDX      2
#define KNOB_4_IDX      3
#define KNOB_5_IDX      4
#define KNOB_6_IDX      5
#define KNOB_EXP_IDX    6

#define NUM_KNOBS       7

// ========== SWITCH INDICES ==========
// Momentary switches (digital inputs)
#define SWITCH_1_IDX    0
#define SWITCH_2_IDX    1
#define SWITCH_3_IDX    2
#define SWITCH_4_IDX    3

#define NUM_SWITCHES    4

// ========== ENCODER INDICES ==========
// Rotary encoders (quadrature inputs)
#define ENCODER_1_IDX   0
#define ENCODER_2_IDX   1

#define NUM_ENCODERS    2

// ========== ENCODER BUTTON INDICES ==========
// Encoder push buttons (offset by NUM_SWITCHES for unique indices)
// These are used as toggle/button controls separate from rotation
#define ENCODER_1_BUTTON_IDX    (NUM_SWITCHES + 0)  // 4
#define ENCODER_2_BUTTON_IDX    (NUM_SWITCHES + 1)  // 5

// ========== LED INDICES ==========
#define LED_1_IDX       0
#define LED_2_IDX       1

#define NUM_LEDS        2

// ========== TOTAL BUTTON INDICES ==========
// Combined switches and encoder buttons
#define NUM_TOTAL_BUTTONS   (NUM_SWITCHES + NUM_ENCODERS)  // 6

} // namespace perspective
