#include "hardware.h"
#include "ui/ui.h"
#include "ui/knob.h"

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

    StartLog(true);

    InitGFX2Display();
   
    InitControls();

    System::Delay(1000); // Allow time for everything to settle

    SetControlUpdateRate(UPDATE_RATE);

    PrintLine("Tick Frequency: %d Hz", System::GetTickFreq());
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

    if (timerRunning) controlTimer.Stop();
    controlTimer.Init(config);
    controlTimer.SetPrescaler(9999);
    controlTimer.SetCallback(timerCallback, this);
    controlTimer.Start();
    controlUpdateRate = rate;

    timerRunning = true;
}

void Hardware::ProcessControls() {
    if (processing) {
        return; // Skip processing if already in the middle of processing to prevent reentrancy issues
    }

    // Update LEDs at a high frequency regardless of event handler to ensure visual responsiveness
    for (int i = 0; i < numLeds; i++) {
        leds[i].Update();
    }

    if (!eventHandler_) {
        return; // No event handler registered, skip processing controls
    }

    // Process knobs and fire knob change events
    if (++knob_divider >= KNOB_DIVISOR) {
        knob_divider = 0;
        for (int i = 0; i < numKnobs; i++) {
            int previousIntValue = knobs[i].GetRawValue();
            knobs[i].Process();
            int newIntValue = knobs[i].GetRawValue();
            int delta = abs(newIntValue - previousIntValue);

            if (delta > 16) {
                eventHandler_->QueueKnobChanged(
                    &knobs[i],
                    i,
                    newIntValue,
                    previousIntValue
                );
            }
        }
    }

    // Process switches and fire button events
    if (++switch_divider >= SWITCH_DIVISOR) {
        switch_divider = 0;
        for (int i = 0; i < numSwitches; i++) {
            switches[i].Process();
            
            if (switches[i].RisingEdge()) {
               // Button pressed - reset hold flag
                switchHoldFired_[i] = false;
                eventHandler_->QueueButtonPressed(
                    &switches[i],
                    i
                );
            } else if (switches[i].FallingEdge()) {
                // Button released
                switchHoldFired_[i] = false;
                eventHandler_->QueueButtonReleased(
                    &switches[i],
                    i
                );
            } else if (switches[i].Pressed()) {
                // Button is being held - check if threshold reached
                int holdTime = switches[i].TimeHeld();
                if (!switchHoldFired_[i] && holdTime >= BUTTON_HOLD_THRESHOLD_MS) {
                    switchHoldFired_[i] = true;
                    eventHandler_->QueueButtonHeld(
                        &switches[i],
                        i,
                        holdTime
                    );
                }
            }
        }
    }

    // Process encoders and fire encoder events
    if (++encoder_divider >= ENCODER_DIVISOR) {
        encoder_divider = 0;
        for (int i = 0; i < numEncoders; i++) {
            encoders[i].Process();
            int increment = encoders[i].Increment();
            
            if (increment != 0) {
                eventHandler_->QueueEncoderChanged(
                    &encoders[i],
                    i,
                    increment
                );
            }
        }
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
        Knob newKnob;
        newKnob.Init(DaisySeed::adc.GetPtr(i), controlUpdateRate / KNOB_DIVISOR);
        knobs.push_back(newKnob);
    }
     
    numSwitches = sizeof(switchPins) / sizeof(Pin);

    for (int i = 0; i < numSwitches; i++) {
        perspective::Switch newSwitch;
        newSwitch.Init(switchPins[i], controlUpdateRate / SWITCH_DIVISOR);
        switches.push_back(newSwitch);
    }

    numEncoders = sizeof(encoderPins) / sizeof(encoderPins[0]);
 
    for (int i = 0; i < numEncoders; i++) {
        perspective::Encoder newEncoder;
        newEncoder.Init(encoderPins[i][0], encoderPins[i][1], encoderPins[i][2], controlUpdateRate / ENCODER_DIVISOR);
        encoders.push_back(newEncoder);
        switches.push_back(*newEncoder.GetSwitch()); // Add encoder button as a switch for event handling
    }

    numSwitches += numEncoders; // Account for encoder buttons added to switches

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

    PrintLine("Starting ADC");
    adc.Start();

    // No Midi controls for now
    /*MidiUartHandler::Config midiConfig;
    midiConfig.transport_config.rx = midiRx;
    midiConfig.transport_config.tx = midiTx;
    midi.Init(midiConfig);*/

    /*MyOledDisplay::Config dispCfg;
    dispCfg.driver_config.transport_config.pin_config.dc = displayDC;
    dispCfg.driver_config.transport_config.pin_config.reset = displayReset;
    display.Init(dispCfg);*/
}

void Hardware::InitGFX2Display()
{
    INIT_DISPLAY(__Display);
    __Display.setOrientation(Rotation::Degre_90);
    DadGFX::cLayer* pBackground = ADD_LAYER(BackgroundLayer, 0, 0, 1);
    pBackground->drawFillRect(0,0,320, 240, DadGFX::sColor(9, 111, 148, 255));
    pBackground->drawFillRect(80, 0, 80, 240, DadGFX::sColor(23, 148, 194, 255));
    pBackground->drawFillRect(240, 0, 240, 240, DadGFX::sColor(23, 148, 194, 255));
  
    __Display.flush();
}
