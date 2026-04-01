#include "hardware.h"
#include "ui/ui.h"
#include "ui/knob.h"

#include "cDisplay.h"
#include "fonts/Arial_14p.h"
#include "fonts/Gill_Sans_Ultra_Bold_72p.h"
#include "fonts/Gill_Sans_Ultra_Bold_36p.h"
#include <cstdio>
#include <cmath>

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
DECLARE_LAYER(TunerOverlay,240,320)

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

    PrintLine("Tick Frequency: %d Hz", System::GetTickFreq());
}

void Hardware::ProcessControls() {

    // Update LEDs at a high frequency regardless of event handler to ensure visual responsiveness
    for (int i = 0; i < numLeds; i++) {
        leds[i].Update();
    }

    if (!eventHandler_) {
        return; // No event handler registered, skip processing controls
    }

    // Poll jack insertion pins (high = jack inserted)
    jackLeftIn_     = leftIn.Read();
    jackRightIn_    = rightIn.Read();
    jackLeftOut_    = leftOut.Read();
    jackRightOut_   = rightOut.Read();
    jackExpression_ = expression.Read();

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
                    // Check if any other switch is currently pressed (combo)
                    int partnerIndex = -1;
                    for (int j = 0; j < numSwitches; j++) {
                        if (j != i && switches[j].Pressed()) {
                            partnerIndex = j;
                            break;
                        }
                    }
                    switchHoldFired_[i] = true;
                    if (partnerIndex >= 0) {
                        // Suppress partner's held and released events too
                        switchHoldFired_[partnerIndex] = true;
                        eventHandler_->QueueButtonsHeldTogether(
                            &switches[i],
                            i,
                            partnerIndex,
                            holdTime
                        );
                    } else {
                        eventHandler_->QueueButtonHeld(
                            &switches[i],
                            i,
                            holdTime
                        );
                    }
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
        newKnob.Init(DaisySeed::adc.GetPtr(i), UPDATE_RATE / KNOB_DIVISOR);

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
        newSwitch.Init(switchPins[i], UPDATE_RATE / SWITCH_DIVISOR);
        switches.push_back(newSwitch);
    }

    numEncoders = sizeof(encoderPins) / sizeof(encoderPins[0]);
 
    for (int i = 0; i < numEncoders; i++) {
        perspective::Encoder newEncoder;
        newEncoder.Init(encoderPins[i][0], encoderPins[i][1], encoderPins[i][2], UPDATE_RATE / ENCODER_DIVISOR);
        newEncoder.SetStepsPerDetent(ENCODER_STEPS_PER_DETENT);
        newEncoder.SetDirection(ENCODER_DIRECTION);
        encoders.push_back(newEncoder);
        switches.push_back(*newEncoder.GetSwitch()); // Add encoder button as a switch for event handling
    }

    numSwitches += numEncoders; // Account for encoder buttons added to switches

    numLeds = sizeof(ledPins) / sizeof(Pin);

    for (int i = 0; i < numLeds; i++) {
        Led newLed;
        newLed.Init(ledPins[i], false, UPDATE_RATE);
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
    GillSans72 = new DadGFX::cFont(&__Gill_Sans_Ultra_Bold_72p);
    GillSans36 = new DadGFX::cFont(&__Gill_Sans_Ultra_Bold_36p);
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

    tunerLayer_ = ADD_LAYER(TunerOverlay, 0, 0, 3);
    tunerLayer_->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    
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

void Hardware::SetStatusDisplay(const char* left, const char* middle, const char* right)
{
    // Layer 9 is reserved for status; three fixed columns across the 220px layer.
    const int kLayerIndex = 9;
    if (kLayerIndex >= static_cast<int>(paramLayers.size())) return;

    DadGFX::cLayer* layer = paramLayers[kLayerIndex];
    layer->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    layer->setTextFrontColor(DadGFX::sColor(255, 255, 255, 255));

    layer->setCursor(0, 0);
    layer->drawText(left);

    layer->setCursor(90, 0);
    layer->drawText(middle);

    layer->setCursor(170, 0);
    layer->drawText(right);

    __Display.flush();
}

void Hardware::SetParameterDisplayHighlighted(int layerIndex, const char* paramName, const char* valueText)
{
    if (layerIndex < 0 || layerIndex >= static_cast<int>(paramLayers.size())) {
        return;
    }

    DadGFX::cLayer* layer = paramLayers[layerIndex];
    layer->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    layer->setTextFrontColor(DadGFX::sColor(255, 220, 50, 255));
    
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

void Hardware::ShowTunerOverlay(const char* noteName, int octave, float centsOffset, float frequency, float referenceFrequency, bool signalDetected)
{
    if(!tunerLayer_ || !GillSans72 || !GillSans36)
    {
        return;
    }

    const DadGFX::sColor bgColor(9, 111, 148, 255);
    const DadGFX::sColor textColor(255, 255, 255, 255);
    const DadGFX::sColor mutedColor(180, 180, 180, 255);
    const DadGFX::sColor sharpColor(255, 159, 64, 255); // orange
    const DadGFX::sColor flatColor(88, 190, 255, 255);  // blue/cyan
    const DadGFX::sColor inTuneColor(88, 190, 255, 255);
    const float inTuneThreshold = 2.5f;
    const float arrowThreshold = 3.0f;
    const float meterRange = 25.0f;

    tunerLayer_->eraseLayer(bgColor);

    const bool hasNote = signalDetected && noteName && noteName[0] != '-' && noteName[1] != '-';

    char statusText[32];
    std::snprintf(statusText, sizeof(statusText), "Tuner");
    tunerLayer_->setFont(Arial14);
    tunerLayer_->setTextFrontColor(textColor);
    tunerLayer_->setCursor((240 - Arial14->getTextWidth(statusText)) / 2, 12);
    tunerLayer_->drawText(statusText);

    // Note + octave
    char noteDisplay[16];
    if(hasNote)
    {
        std::snprintf(noteDisplay, sizeof(noteDisplay), "%s%d", noteName, octave);
    }
    else
    {
        std::snprintf(noteDisplay, sizeof(noteDisplay), "-");
    }

    tunerLayer_->setFont(GillSans72);
    tunerLayer_->setTextFrontColor(textColor);
    int noteTextWidth = GillSans72->getTextWidth(noteDisplay);
    tunerLayer_->setCursor((240 - noteTextWidth) / 2, 52);
    tunerLayer_->drawText(noteDisplay);

    // Cents meter
    const int meterX = 24;
    const int meterY = 224;
    const int meterW = 192;
    const int meterH = 18;
    const int centerX = meterX + meterW / 2;
    tunerLayer_->drawFillRect(meterX, meterY, meterW, meterH, DadGFX::sColor(15, 35, 45, 255));
    tunerLayer_->drawFillRect(centerX - 1, meterY - 4, 3, meterH + 8, textColor);

    if(hasNote)
    {
        const float absCents = fabsf(centsOffset);
        const bool isSharp = centsOffset > arrowThreshold;
        const bool inTune = absCents <= inTuneThreshold;
        const DadGFX::sColor activeColor = inTune ? inTuneColor : (isSharp ? sharpColor : flatColor);

        float clamped = centsOffset;
        if(clamped < -meterRange) clamped = -meterRange;
        if(clamped > meterRange) clamped = meterRange;
        int offsetPx = static_cast<int>((clamped / meterRange) * (meterW / 2 - 6));
        if(offsetPx >= 0)
        {
            tunerLayer_->drawFillRect(centerX, meterY + 2, offsetPx, meterH - 4, activeColor);
        }
        else
        {
            tunerLayer_->drawFillRect(centerX + offsetPx, meterY + 2, -offsetPx, meterH - 4, activeColor);
        }
    }

    // Bottom text
    if(hasNote)
    {
        char freqStr[32];
        snprintf(freqStr, sizeof(freqStr), "REF %.1f Hz", referenceFrequency);
            tunerLayer_->setFont(Arial14);
        tunerLayer_->setTextFrontColor(textColor);
            tunerLayer_->setCursor((240 - Arial14->getTextWidth(freqStr)) / 2, 286);
        tunerLayer_->drawText(freqStr);
    }
    else
    {
        char freqStr[32];
        snprintf(freqStr, sizeof(freqStr), "REF %.1f Hz", referenceFrequency);
            tunerLayer_->setFont(Arial14);
        tunerLayer_->setTextFrontColor(mutedColor);
            tunerLayer_->setCursor((240 - Arial14->getTextWidth(freqStr)) / 2, 286);
        tunerLayer_->drawText(freqStr);
    }

    __Display.flush();
}

void Hardware::HideTunerOverlay()
{
    if(!tunerLayer_)
    {
        return;
    }

    tunerLayer_->eraseLayer(DadGFX::sColor(0, 0, 0, 0));
    __Display.flush();
}
