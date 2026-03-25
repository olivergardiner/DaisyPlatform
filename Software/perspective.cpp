#include "perspective.h"
#include "effects/effect.h"
#include "parameters/effectparameter.h"
#include "parameters/potentiometerparameter.h"
#include "parameters/encoderparameter.h"
#include "parameters/toggleparameter.h"
#include "effects/effectfactory.h"

using namespace perspective;

static Perspective* g_perspective = nullptr;

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    if (g_perspective) {
        g_perspective->AudioCallbackImpl(in, out, size);
    }
}

Perspective::Perspective() 
    : currentEffect_(nullptr) {
    g_perspective = this;
}

Perspective::~Perspective() {

}

void Perspective::Init() {
    hardware.Init(GetEventHandler());

    LoadEffects(); // Load effects before registering listeners so we can populate effect selection menu

    // Initialize perspective-specific UI elements
    RegisterEventListeners();

    hardware.StartAudio(AudioCallback);
}

void Perspective::Exec() {
    while(true) {
        hardware.SetProcessing(true); // Indicate that we're processing controls/events

        eventHandler_.ProcessEvents();
        
        // Update tuner display when in tuner mode
        if (tunerMode_) {
            UpdateTunerDisplay();
        }

        hardware.SetProcessing(false); // Done processing controls/events

        hardware.DelayMs(1); // Small delay to allow events to accumulate
    }
}     
void Perspective::AudioCallbackImpl(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    float ledPulseBrightness = 0.0f;

    if (tunerMode_ && tunerEffect_) {
        // Tuner mode: process for pitch detection but mute output
        tunerEffect_->ProcessStereo(const_cast<float*>(in[0]), const_cast<float*>(in[1]), out[0], out[1], size);
        for (size_t i = 0; i < size; i++) {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    } else if (currentEffect_ && !bypassMode_) {
        // Process with current effect
        // Note: ProcessStereo requires non-const pointers, but won't modify input
        currentEffect_->ProcessStereo(const_cast<float*>(in[0]), const_cast<float*>(in[1]), out[0], out[1], size);
        ledPulseBrightness = currentEffect_->GetTempoPulseBrightness();
    } else if (bypassMode_) {
        // Bypass mode
        if (bypassType_ == BypassType::PASSTHROUGH) {
            // Passthrough - pass audio unprocessed
            for (size_t i = 0; i < size; i++){
                out[0][i] = in[0][i];
                out[1][i] = in[1][i];
            }
        } else {
            // True bypass - hardware relay handles routing, pass through DSP
            // (Audio is routed by hardware when TRUE_BYPASS_PIN is driven low)
            for (size_t i = 0; i < size; i++){
                out[0][i] = in[0][i];
                out[1][i] = in[1][i];
            }
        }
    } else {
        // Fallback - pass through
        for (size_t i = 0; i < size; i++){
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    }
    hardware.SetLedBrightness(LED_1_IDX, ledPulseBrightness);
}

void Perspective::RegisterEventListeners() {
    // Register event listeners, setup display, etc.
    
    // Generic listener for knob changes - updates effect parameters
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (!currentEffect_) return;
            
            // Find parameter with matching index
            for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
                EffectParameter* param = currentEffect_->GetParameter(i);
                if (param && param->GetIndex() == event.controlIndex) {
                    // Update parameter based on type
                    if (param->GetType() == ParameterType::POTENTIOMETER) {
                        PotentiometerParameter* potParam = static_cast<PotentiometerParameter*>(param);
                        Knob* knob = static_cast<Knob*>(event.source);
                        float normalizedValue = knob->Value(); // Get processed value from knob
                        potParam->SetNormalizedValueWithCurve(normalizedValue);
                        
                        // Update display (only if not hidden)
                        if (param->GetDisplayIndex() >= 0) {
                            UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
                        }
                        
                        //Hardware::PrintLine("%s: %d", potParam->GetName(), static_cast<int>(event.value * 100));

                        // Update effect with new parameter value
                        currentEffect_->Update();
                        break;
                    }
                }
            }
        },
        UIEventType::KNOB_CHANGED
    );
    
    // Generic listener for encoder changes - updates effect parameters
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (!currentEffect_) return;
            
            // Find parameter with matching index
            for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
                EffectParameter* param = currentEffect_->GetParameter(i);
                if (param && param->GetIndex() == event.controlIndex) {
                    // Update parameter based on type
                    if (param->GetType() == ParameterType::ENCODER) {
                        EncoderParameter* encParam = static_cast<EncoderParameter*>(param);
                        if (event.value > 0) {
                            encParam->Increment(event.value);
                        } else if (event.value < 0) {
                            encParam->Decrement(-event.value);
                        }
                        
                        // Update display (only if not hidden)
                        if (param->GetDisplayIndex() >= 0) {
                            UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
                        }
                        
                        //Hardware::PrintLine("%s: %f", encParam->GetName(), encParam->GetValue());

                        // Update effect with new parameter value
                        currentEffect_->Update();
                        break;
                    }
                }
            }
        },
        UIEventType::ENCODER_CHANGED
    );
    
    // Generic listener for button presses - updates toggle parameters
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (!currentEffect_) return;
            
            // Find parameter with matching index
            for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
                EffectParameter* param = currentEffect_->GetParameter(i);
                if (param && param->GetIndex() == event.controlIndex) {
                    // Update parameter based on type
                    if (param->GetType() == ParameterType::TOGGLE) {
                        ToggleParameter* toggleParam = static_cast<ToggleParameter*>(param);
                        toggleParam->Toggle();
                        
                        // Update display (only if not hidden)
                        if (param->GetDisplayIndex() >= 0) {
                            UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
                        }
                        
                        Hardware::PrintLine("%s: %d", toggleParam->GetName(), static_cast<int>(toggleParam->GetValue()));

                        // Update effect with new parameter value
                        currentEffect_->Update();
                        break;
                    }
               }
            }
        },
        UIEventType::BUTTON_RELEASED
    );
    
    // Register listener for Switch_1 being pressed (next effect)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (effects_.empty()) return;
            
            // Navigate to next effect (wrap around)
            if (currentEffectIndex_ < effects_.size() - 1) {
                SetCurrentEffect(currentEffectIndex_ + 1);
            } else {
                SetCurrentEffect(0); // Wrap to first effect
            }
        },
        UIEventType::BUTTON_RELEASED,
        0  // Index 0 = Switch_1
    );
    
    // Register listener for Switch_1 being held (tuner mode toggle)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (tunerMode_) {
                ExitTunerMode();
            } else {
                EnterTunerMode();
            }
        },
        UIEventType::BUTTON_HELD,
        0  // Index 0 = Switch_1
    );
    
    // Register listener for Switch_2 being pressed (previous effect)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (effects_.empty()) return;
            
            // Navigate to previous effect (wrap around)
            if (currentEffectIndex_ > 0) {
                SetCurrentEffect(currentEffectIndex_ - 1);
            } else {
                SetCurrentEffect(effects_.size() - 1); // Wrap to last effect
            }
        },
        UIEventType::BUTTON_RELEASED,
         1  // Index 1 = Switch_2
   );
    
    // Register listener for Switch_3 being pressed (bypass toggle)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            ToggleBypass();
        },
        UIEventType::BUTTON_RELEASED,
        2  // Index 2 = Switch_3
    );
    
    // Register listener for Switch_3 being held (bypass type toggle)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            ToggleBypassType();
        },
        UIEventType::BUTTON_HELD,
        2  // Index 2 = Switch_3
    );
    
    // Register listener for Switch_4 being pressed (tap tempo)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            HandleTapTempo();
        },
        UIEventType::BUTTON_RELEASED,
        3  // Index 3 = Switch_4
    );
    
    // Register listener for Switch_4 being held (metronome toggle)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            ToggleMetronome();
        },
        UIEventType::BUTTON_HELD,
        3  // Index 3 = Switch_4
    );

}

void Perspective::ToggleBypass() {
    bypassMode_ = !bypassMode_;
    
    // Control hardware true bypass relay
    if (bypassMode_ && bypassType_ == BypassType::TRUE_BYPASS) {
        hardware.SetTrueBypass(true);  // Drive pin LOW to activate relay
    } else {
        hardware.SetTrueBypass(false); // Drive pin HIGH to deactivate relay
    }
    
    UpdateStatusDisplay();
}

void Perspective::ToggleBypassType() {
    // Toggle between true bypass and passthrough
    if (bypassType_ == BypassType::TRUE_BYPASS) {
        bypassType_ = BypassType::PASSTHROUGH;
    } else {
        bypassType_ = BypassType::TRUE_BYPASS;
    }
    
    // Update hardware relay state
    if (bypassMode_ && bypassType_ == BypassType::TRUE_BYPASS) {
        hardware.SetTrueBypass(true);  // Drive pin LOW to activate relay
    } else {
        hardware.SetTrueBypass(false); // Drive pin HIGH to deactivate relay
    }
    
    UpdateStatusDisplay();
    hardware.ForceKnobValueChangedEvents();
}

void Perspective::HandleTapTempo() {
    if (!currentEffect_) return;
    
    uint32_t currentTime = hardware.system.GetNow();
    
    // Check if this is a valid tap (within timeout)
    if (lastTapTime_ > 0 && (currentTime - lastTapTime_) < TAP_TIMEOUT_MS) {
        // Calculate interval between taps
        tapInterval_ = currentTime - lastTapTime_;
        
        // Convert interval to frequency (Hz)
        // interval is in milliseconds, so frequency = 1000 / interval
        float tempoHz = 1000.0f / static_cast<float>(tapInterval_);
        
        // Set tempo on effect
        currentEffect_->SetTempo(tempoHz);
    }
    
    // Store current tap time for next tap
    lastTapTime_ = currentTime;
}

void Perspective::ToggleMetronome() {
    if (!currentEffect_) return;
    
    // Toggle global metronome state
    metronomeEnabled_ = !metronomeEnabled_;
    
    // Propagate to current effect
    currentEffect_->SetMetronomeEnabled(metronomeEnabled_);
    currentEffect_->Update();
    
    UpdateStatusDisplay();
    Hardware::PrintLine("Metronome: %s", metronomeEnabled_ ? "ON" : "OFF");
}

void Perspective::LoadEffects() {
    // Populate effects vector using the factory function
    float sampleRate = hardware.AudioSampleRate();
    PopulateEffects(&effects_, sampleRate);
    
    // Initialize tuner separately (not part of effects list)
    tunerEffect_ = new TunerEffect();
    tunerEffect_->Init(sampleRate);
    
    // Set display update callback for all effects
    for (auto* effect : effects_) {
        effect->SetDisplayUpdateCallback([this](EffectParameter* param, size_t displayIndex) {
            this->UpdateParameterDisplay(param, displayIndex + 1); // +1 to skip effect name display
        });
    }
    
    // Set the first effect as current
    SetCurrentEffect(0);
}

void Perspective::SetCurrentEffect(size_t index) {
    if (index >= effects_.size()) {
        return; // Invalid index
    }

    if (currentEffect_) {
        currentEffect_->OnDeselected();
    }
    
    currentEffectIndex_ = index;
    currentEffect_ = effects_[index];
    currentEffect_->OnSelected();
    
    // Propagate global metronome state to new effect (propagates through compound effect hierarchy)
    currentEffect_->SetMetronomeEnabled(metronomeEnabled_);
    
    // Update effect to apply metronome state propagation through compound effect children
    currentEffect_->Update();
    
    // Clear the display before showing new effect
    hardware.ClearDisplay();
    
    hardware.SetParameterDisplay(0, currentEffect_->GetName(), "");
    
    // Display initial values for visible parameters only
    for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
        EffectParameter* param = currentEffect_->GetParameter(i);
        // Only display parameters with displayIndex >= 0 (not hidden)
        if (param && param->GetDisplayIndex() >= 0) {
            UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
        }
    }
    
    UpdateStatusDisplay();
}

void Perspective::UpdateParameterDisplay(EffectParameter* param, size_t displayIndex) {
    if (!param) return;
    
    char valueStr[16];
    param->GetValueAsString(valueStr, sizeof(valueStr));
    hardware.SetParameterDisplay(displayIndex, param->GetName(), valueStr);
}

void Perspective::UpdateStatusDisplay() {
    const char* bypassText = "";
    if (bypassMode_) {
        bypassText = (bypassType_ == BypassType::TRUE_BYPASS) ? "Bypass" : "Pass Thru";
    }
    const char* metroText = metronomeEnabled_ ? "Met" : "";
    hardware.SetParameterDisplay(9, bypassText, metroText);
}

void Perspective::EnterTunerMode() {
    if (!tunerEffect_) return;
    
    tunerMode_ = true;
    bypassMode_ = false;  // Disable bypass when entering tuner mode
    
    // Clear display and show tuner interface
    hardware.ClearDisplay();
    hardware.SetParameterDisplay(0, "TUNER MODE", "");
    
    // Initial tuner display
    UpdateTunerDisplay();
}

void Perspective::ExitTunerMode() {
    tunerMode_ = false;
    
    // Restore current effect display
    SetCurrentEffect(currentEffectIndex_);
}

void Perspective::UpdateTunerDisplay() {
    if (!tunerEffect_ || !tunerMode_) return;
    
    char noteStr[16];
    char freqStr[16];
    char centsStr[16];
    
    if (tunerEffect_->IsSignalDetected()) {
        // Format note name with octave
        snprintf(noteStr, sizeof(noteStr), "%s%d", 
                 tunerEffect_->GetNoteName(), 
                 tunerEffect_->GetNoteOctave());
        
        // Format frequency
        snprintf(freqStr, sizeof(freqStr), "%.1f Hz", 
                 tunerEffect_->GetDetectedFrequency());
        
        // Format cents offset with sign
        float cents = tunerEffect_->GetCentsOffset();
        if (cents > 0) {
            snprintf(centsStr, sizeof(centsStr), "+%.0f", cents);
        } else {
            snprintf(centsStr, sizeof(centsStr), "%.0f", cents);
        }
    } else {
        snprintf(noteStr, sizeof(noteStr), "--");
        snprintf(freqStr, sizeof(freqStr), "---");
        snprintf(centsStr, sizeof(centsStr), "--");
    }
    
    hardware.SetParameterDisplay(1, "Note", noteStr);
    hardware.SetParameterDisplay(2, "Frequency", freqStr);
    hardware.SetParameterDisplay(3, "Cents", centsStr);
}

