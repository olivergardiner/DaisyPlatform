#include "hardware.h"
#include "ui/ui.h"

#include "cDisplay.h"

using namespace daisy;
using namespace perspective;

DECLARE_DISPLAY(__Display);
DECLARE_LAYER(BackgroundLayer, 320, 240)

void timerCallback(void* data)
{
    Hardware* hardware = static_cast<Hardware *>(data);
    hardware->ProcessControls();
}

Hardware::Hardware() {}

Hardware::~Hardware() {}

void Hardware::Init(UIEventHandler* eventHandler)
{
    eventHandler_ = eventHandler;
    DaisySeed::Init(boost);
    
    InitControls();
}

void perspective::Hardware::SetControlUpdateRate(float rate)
{
    for (int i = 0; i < numKnobs; i++) {
        knobs[i].SetSampleRate(rate);
    }

    for (int i = 0; i < numSwitches; i++) {
        switches[i].SetUpdateRate(rate);
    }

    for (int i = 0; i < numEncoders; i++) {
        encoders[i].SetUpdateRate(rate);
    }

    for (int i = 0; i < numLeds; i++) {
        leds[i].SetSampleRate(rate);
    }

    float tickRate = (boost) ? 24000.0f : 20000.0f; //NB: These values are for a prescaler of 9999 (i.e. the default tick rate / 10000)
    int period = static_cast<int>(tickRate / rate) - 1;

    TimerHandle::Config config;
    config.dir = TimerHandle::Config::CounterDir::UP;
    config.enable_irq = true;
    config.period = period;
    config.periph = TimerHandle::Config::Peripheral::TIM_5;

    controlTimer.Stop();
    controlTimer.Init(config);
    controlTimer.SetPrescaler(9999);
    controlTimer.SetCallback(timerCallback, this);
    controlTimer.Start();

    controlUpdateRate = rate;
}

void Hardware::ProcessControls()
{
    if (!eventHandler_) {
        // If no event handler, just process controls without firing events
        for (int i = 0; i < numKnobs; i++) {
            knobs[i].Process();
        }

        for (int i = 0; i < numSwitches; i++) {
            switches[i].Debounce();
        }

        for (int i = 0; i < numEncoders; i++) {
            encoders[i].Debounce();
        }

        for (int i = 0; i < numLeds; i++) {
            leds[i].Update();
        }
        return;
    }

    // Process knobs and fire knob change events
    for (int i = 0; i < numKnobs; i++) {
        float previousValue = knobs[i].Value();
        knobs[i].Process();
        float newValue = knobs[i].Value();
        
        if (newValue != previousValue) {
            eventHandler_->QueueKnobChanged(
                &knobs[i],
                i,
                newValue,
                previousValue,
                "Knob_" + std::to_string(i + 1)
            );
        }
    }

    // Process switches and fire button events
    for (int i = 0; i < numSwitches; i++) {
        bool previousState = switches[i].Pressed();
        switches[i].Debounce();
        bool currentState = switches[i].Pressed();
        
        if (currentState && !previousState) {
            // Button pressed - reset hold flag
            switchHoldFired_[i] = false;
            eventHandler_->QueueButtonPressed(
                &switches[i],
                i,
                "Switch_" + std::to_string(i + 1)
            );
        } else if (!currentState && previousState) {
            // Button released
            switchHoldFired_[i] = false;
            eventHandler_->QueueButtonReleased(
                &switches[i],
                i,
                "Switch_" + std::to_string(i + 1)
            );
        } else if (currentState) {
            // Button is being held - check if threshold reached
            float holdTime = switches[i].TimeHeldMs();
            if (!switchHoldFired_[i] && holdTime >= BUTTON_HOLD_THRESHOLD_MS) {
                switchHoldFired_[i] = true;
                eventHandler_->QueueButtonHeld(
                    &switches[i],
                    i,
                    "Switch_" + std::to_string(i + 1),
                    holdTime
                );
            }
        }
    }

    // Process encoders and fire encoder events
    for (int i = 0; i < numEncoders; i++) {
        encoders[i].Debounce();
        int increment = encoders[i].Increment();
        
        if (increment != 0) {
            eventHandler_->QueueEncoderChanged(
                &encoders[i],
                i,
                increment,
                "Encoder_" + std::to_string(i + 1)
            );
        }
        
        // Process encoder button (switch) events using native Encoder methods
        if (encoders[i].RisingEdge()) {
            // Encoder button just pressed - reset hold flag
            encoderHoldFired_[i] = false;
            eventHandler_->QueueButtonPressed(
                &encoders[i],
                i + numSwitches,  // Offset by number of switches for unique index
                "Encoder_" + std::to_string(i + 1) + "_Button"
            );
        } else if (encoders[i].FallingEdge()) {
            // Encoder button released
            encoderHoldFired_[i] = false;
            eventHandler_->QueueButtonReleased(
                &encoders[i],
                i + numSwitches,  // Offset by number of switches for unique index
                "Encoder_" + std::to_string(i + 1) + "_Button"
            );
        } else if (encoders[i].Pressed()) {
            // Encoder button is being held - check if threshold reached
            float holdTime = encoders[i].TimeHeldMs();
            if (!encoderHoldFired_[i] && holdTime >= BUTTON_HOLD_THRESHOLD_MS) {
                encoderHoldFired_[i] = true;
                eventHandler_->QueueButtonHeld(
                    &encoders[i],
                    i + numSwitches,  // Offset by number of switches for unique index
                    "Encoder_" + std::to_string(i + 1) + "_Button",
                    holdTime
                );
            }
        }
    }

    // Update LEDs (no events needed)
    for (int i = 0; i < numLeds; i++) {
        leds[i].Update();
    }
}

void Hardware::InitControls()
{
    numKnobs = sizeof(knobPins) / sizeof(Pin);

    AdcChannelConfig cfg[numKnobs];

    for (int i = 0; i < numKnobs; i++) {
        cfg[i].InitSingle(knobPins[i]);
    }

    DaisySeed::adc.Init(cfg, numKnobs);

    for (int i = 0; i < numKnobs; i++) {
        AnalogControl newKnob;
        newKnob.Init(DaisySeed::adc.GetPtr(i), controlUpdateRate);
        knobs.push_back(newKnob);
    }
     
    numSwitches = sizeof(switchPins) / sizeof(Pin);

    for (int i = 0; i < numSwitches; i++) {
        NoPullSwitch newSwitch;
        newSwitch.Init(switchPins[i]);
        switches.push_back(newSwitch);
    }

    numEncoders = sizeof(encoderPins) / sizeof(encoderPins[0]);
 
    for (int i = 0; i < numEncoders; i++) {
        NoPullEncoder newEncoder;
        newEncoder.Init(encoderPins[i][0], encoderPins[i][1], encoderPins[i][2], controlUpdateRate);
        encoders.push_back(newEncoder);
    }

    numLeds = sizeof(ledPins) / sizeof(Pin);

    for (int i = 0; i < numLeds; i++) {
        Led newLed;
        newLed.Init(ledPins[i], false, controlUpdateRate);
        leds.push_back(newLed);
    }

    // Jack insertion detection
    leftIn.Init(AUDIO_IN_L_PIN, GPIO::Mode::INPUT);
    rightIn.Init(AUDIO_IN_R_PIN, GPIO::Mode::INPUT);
    leftOut.Init(AUDIO_OUT_L_PIN, GPIO::Mode::INPUT);
    rightOut.Init(AUDIO_OUT_R_PIN, GPIO::Mode::INPUT);
    expression.Init(EXPRESSION_PEDAL_PIN, GPIO::Mode::INPUT);

    // True bypass control
    trueBypass.Init(TRUE_BYPASS_PIN, GPIO::Mode::OUTPUT);

    adc.Start();

    SetControlUpdateRate(controlUpdateRate); // 1 kHz update rate

    // No Midi controls for now
    /*MidiUartHandler::Config midiConfig;
    midiConfig.transport_config.rx = midiRx;
    midiConfig.transport_config.tx = midiTx;
    midi.Init(midiConfig);*/

    /*MyOledDisplay::Config dispCfg;
    dispCfg.driver_config.transport_config.pin_config.dc = displayDC;
    dispCfg.driver_config.transport_config.pin_config.reset = displayReset;
    display.Init(dispCfg);*/

    INIT_DISPLAY(__Display);
    DadGFX::cLayer* pBackground = ADD_LAYER(BackgroundLayer, 0, 0, 1);
    pBackground->drawFillRect(0,0,320, 240, DadGFX::sColor(9, 111, 148, 255));
    pBackground->drawFillRect(80, 0, 80, 240, DadGFX::sColor(23, 148, 194, 255));
    pBackground->drawFillRect(240, 0, 240, 240, DadGFX::sColor(23, 148, 194, 255));
  
    __Display.flush();
}
