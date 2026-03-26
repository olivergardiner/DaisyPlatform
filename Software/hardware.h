#ifndef PERSPECTIVE_HARDWARE_H
#define PERSPECTIVE_HARDWARE_H

#include "daisy_seed.h"
#include "hid/logger.h"
#include <vector>

#include "hardware_pins.h"
#include "ui/knob.h"
#include "ui/switch.h"
#include "ui/encoder.h"

using namespace daisy;

#define UPDATE_RATE 1000.0f // in Hz
#define ENCODER_DIVISOR 1 // Process encoder changes every control tick for best quadrature tracking
#define ENCODER_STEPS_PER_DETENT 2 // 1=quarter-step, 2=half-step, 4=full detent
#define ENCODER_DIRECTION 1 // Set to 1 or -1 to match physical clockwise/counter-clockwise direction
#define SWITCH_DIVISOR 1 // Process switches every control tick for responsive edge detection
#define KNOB_DIVISOR 16 // Process knob changes every 16th call to control timer for better responsiveness

namespace DadGFX {
    class cLayer;
    class cFont;
}

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
        void ForceKnobValueChangedEvents();
        void SetControlUpdateRate(float rate);
        inline void SetProcessing(bool processing) { this->processing = processing; }
        inline void SetLedBrightness(int index, float brightness) {
            if (index >= 0 && index < numLeds) {
                leds[index].Set(brightness);
            }
        }
        
        inline void SetTrueBypass(bool active) {
            // Active = true means activate true bypass (drive pin LOW)
            // Active = false means deactivate (drive pin HIGH)
            trueBypass.Write(!active);
        }

        inline Knob* GetKnob(int index) {
            if (index >= 0 && index < numKnobs) {
                return &knobs[index];
            }
            return nullptr;
        }

        inline perspective::Switch* GetSwitch(int index) {
            if (index >= 0 && index < numSwitches) {
                return &switches[index];
            }
            return nullptr;
        }

        inline perspective::Encoder* GetEncoder(int index) {
            if (index >= 0 && index < numEncoders) {
                return &encoders[index];
            }
            return nullptr;
        }

        inline Led* GetLed(int index) {
            if (index >= 0 && index < numLeds) {
                return &leds[index];
            }
            return nullptr;
        }

        void SetParameterDisplay(int layerIndex, const char* paramName, const char* valueText);
        void ClearDisplay();

    protected:
        void InitControls();
        void InitGFX2Display();

        bool boost = true;
        float controlUpdateRate = 0.0f; // in Hz
        bool timerRunning=false;
        bool processing = false;

        std::vector<Knob> knobs;
        std::vector<perspective::Switch> switches;
        std::vector<perspective::Encoder> encoders;
        std::vector<Led> leds;

        int numKnobs = 0;
        int numSwitches = 0;
        int numEncoders = 0;
        int numLeds = 0;

        int encoder_divider = 0;
        int switch_divider = 0;
        int knob_divider = 0;

        UIEventHandler* eventHandler_;

        std::vector<DadGFX::cLayer*> paramLayers;
        DadGFX::cFont* Arial14;

        // Track if hold event has been fired for each switch
        bool switchHoldFired_[6] = {false, false, false, false, false, false};
        static constexpr int BUTTON_HOLD_THRESHOLD_MS = 1000;

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

        int knobValues_[7] = {0};
    };
}

#endif // PERSPECTIVE_HARDWARE_H