#include "hardware.h"
#include "ui/ui.h"
#include "ui/knob.h"

#include "cDisplay.h"
#include "fonts/Arial_14p.h"

using namespace daisy;
using namespace perspective;

DECLARE_DISPLAY(__Display);
DECLARE_LAYER(BackgroundLayer, 240, 320)
DECLARE_LAYER(EffectName,220,32)
DECLARE_LAYER(ParamLayer_1,220,32)
DECLARE_LAYER(ParamLayer_2,220,32)
DECLARE_LAYER(ParamLayer_3,220,32)
DECLARE_LAYER(ParamLayer_4,220,32)
DECLARE_LAYER(ParamLayer_5,220,32)
DECLARE_LAYER(ParamLayer_6,220,32)
DECLARE_LAYER(ParamLayer_7,220,32)
DECLARE_LAYER(ParamLayer_8,220,32)
DECLARE_LAYER(ParamLayer_9,220,32)

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

    StartLog(false);

    InitGFX2Display();
   
    InitControls();

    System::Delay(1000); // Allow time for everything to settle

    SetControlUpdateRate(UPDATE_RATE);

    PrintLine("Tick Frequency: %d Hz", System::GetTickFreq());
}

void perspective::Hardware::SetControlUpdateRate(float rate)
{
    PrintLine("Setting control update rate to %.2f Hz", rate);
    for (int i = 0; i < numKnobs; i++) {
        knobs[i].SetSampleRate(rate / KNOB_DIVISOR);
    }

    for (int i = 0; i < numSwitches; i++) {
        switches[i].SetUpdateRate(rate / SWITCH_DIVISOR);
    }

    for (int i = 0; i < numEncoders; i++) {
        encoders[i].SetUpdateRate(rate / ENCODER_DIVISOR);
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
    // Control scanning runs in the main loop to avoid ISR interaction with UI/event processing.
    //controlTimer.Start();
    controlUpdateRate = rate;

    timerRunning = true;
}

void Hardware::ProcessControls() {

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

            if (delta > 8) { // Only fire event if change is significant to avoid noise
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
                // Button released - only fire if hold event was not triggered
                if (!switchHoldFired_[i]) {
                    eventHandler_->QueueButtonReleased(
                        &switches[i],
                        i
                    );
                }
                // Reset hold flag after release
                switchHoldFired_[i] = false;
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

void Hardware::ForceKnobValueChangedEvents()
{
    if(!eventHandler_)
    {
        return;
    }

    for(int i = 0; i < numKnobs; i++)
    {
        knobs[i].Process();
        int currentValue = knobs[i].GetRawValue();
        eventHandler_->QueueKnobChanged(&knobs[i], i, currentValue, currentValue);
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

        // Expression pedal calibration: observed physical sweep is ~30%..95%.
        // Map that range to normalized 0..1 for effect parameters.
        if (i == numKnobs - 1) {
            newKnob.SetScale(1.0f / 0.65f); // 1 / (0.95 - 0.30)
            newKnob.SetOffset(0.30f);
        }

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
        newEncoder.SetStepsPerDetent(ENCODER_STEPS_PER_DETENT);
        newEncoder.SetDirection(ENCODER_DIRECTION);
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
    trueBypass.Write(true); // Start with true bypass off

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
    //__Display.setOrientation(Rotation::Degre_90);
    DadGFX::cLayer* pBackground = ADD_LAYER(BackgroundLayer, 0, 0, 1);
    pBackground->drawFillRect(0,0,240, 320, DadGFX::sColor(9, 111, 148, 255));

    Arial14 = new DadGFX::cFont(&__Arial_14p);
    // Add EffectName layer and ParamLayers at 32 pixel intervals
    DadGFX::cLayer* pEffectName = ADD_LAYER(EffectName, 10, 0, 2);
    pEffectName->setFont(Arial14);
    paramLayers.push_back(pEffectName);
    DadGFX::cLayer* pParam1 = ADD_LAYER(ParamLayer_1, 10, 32, 2);
    pParam1->setFont(Arial14);
    paramLayers.push_back(pParam1);
    DadGFX::cLayer* pParam2 = ADD_LAYER(ParamLayer_2, 10, 64, 2);
    pParam2->setFont(Arial14);
    paramLayers.push_back(pParam2);
    DadGFX::cLayer* pParam3 = ADD_LAYER(ParamLayer_3, 10, 96, 2);
    pParam3->setFont(Arial14);
    paramLayers.push_back(pParam3);
    DadGFX::cLayer* pParam4 = ADD_LAYER(ParamLayer_4, 10, 128, 2);
    pParam4->setFont(Arial14);
    paramLayers.push_back(pParam4);
    DadGFX::cLayer* pParam5 = ADD_LAYER(ParamLayer_5, 10, 160, 2);
    pParam5->setFont(Arial14);
    paramLayers.push_back(pParam5);
    DadGFX::cLayer* pParam6 = ADD_LAYER(ParamLayer_6, 10, 192, 2);
    pParam6->setFont(Arial14);
    paramLayers.push_back(pParam6);
    DadGFX::cLayer* pParam7 = ADD_LAYER(ParamLayer_7, 10, 224, 2);
    pParam7->setFont(Arial14);
    paramLayers.push_back(pParam7);
    DadGFX::cLayer* pParam8 = ADD_LAYER(ParamLayer_8, 10, 256, 2);
    pParam8->setFont(Arial14);
    paramLayers.push_back(pParam8);
    DadGFX::cLayer* pParam9 = ADD_LAYER(ParamLayer_9, 10, 288, 2);
    pParam9->setFont(Arial14);
    paramLayers.push_back(pParam9);
    
    __Display.flush();
}

void Hardware::SetParameterDisplay(int layerIndex, const char* paramName, const char* valueText)
{
    if (layerIndex < 0 || layerIndex >= static_cast<int>(paramLayers.size())) {
        return; // Invalid index
    }

    DadGFX::cLayer* layer = paramLayers[layerIndex];
    layer->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    layer->setTextFrontColor(DadGFX::sColor(255, 255, 255, 255));
    
    // Draw parameter name
    layer->setCursor(0, 0);
    layer->drawText(paramName);
    
    // Draw value at 160 pixels to the right
    layer->setCursor(160, 0);
    layer->drawText(valueText);
    
    __Display.flush();
}

void Hardware::ClearDisplay()
{
    // Clear all parameter layers
    for (auto* layer : paramLayers) {
        layer->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    }
    __Display.flush();
}
