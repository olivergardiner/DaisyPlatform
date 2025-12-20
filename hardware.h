#pragma once

#include "daisy_seed.h"
#include <vector>

#include "hardware_pins.h"
#include "nopullcontrols.h"

using namespace daisy;

namespace perspective {
    // Forward declaration
    class UIEventHandler;
    
    class Hardware : public DaisySeed
    {
    public:
        Hardware();
        ~Hardware();

        void Init(UIEventHandler* eventHandler = nullptr);
        void ProcessControls();
        void SetControlUpdateRate(float rate);
    
    protected:
        void InitControls();

        bool boost = true;
        float controlUpdateRate = 1000.0f; // in Hz

        std::vector<AnalogControl> knobs;
        std::vector<NoPullSwitch> switches;
        std::vector<NoPullEncoder> encoders;
        std::vector<Led> leds;

        int numKnobs = 0;
        int numSwitches = 0;
        int numEncoders = 0;
        int numLeds = 0;

        UIEventHandler* eventHandler_;

        // Track if hold event has been fired for each switch
        bool switchHoldFired_[4] = {false, false, false, false};
        static constexpr float BUTTON_HOLD_THRESHOLD_MS = 2000.0f;
        
        // Track if hold event has been fired for each encoder button
        bool encoderHoldFired_[2] = {false, false};

        Pin knobPins[7] = {KNOB_1_PIN, KNOB_2_PIN, KNOB_3_PIN, KNOB_4_PIN, KNOB_5_PIN, KNOB_6_PIN, KNOB_EXP_PIN};
        Pin switchPins[4] = {SWITCH_1_PIN, SWITCH_2_PIN, SWITCH_3_PIN, SWITCH_4_PIN};
        Pin encoderPins[2][3] = {{ENCODER_1_A_PIN, ENCODER_1_B_PIN, ENCODER_1_BUTTON_PIN}, {ENCODER_2_A_PIN, ENCODER_2_B_PIN, ENCODER_2_BUTTON_PIN}};
        Pin ledPins[2] = {LED_1_PIN, LED_2_PIN};

        GPIO leftIn;
        GPIO rightIn;
        GPIO leftOut;
        GPIO rightOut;
        GPIO expression;
        GPIO trueBypass;

        TimerHandle controlTimer;
    };
}