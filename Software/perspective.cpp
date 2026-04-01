#include "perspective.h"
#include "effects/effect.h"
#include "parameters/effectparameter.h"
#include "parameters/potentiometerparameter.h"
#include "parameters/encoderparameter.h"
#include "parameters/toggleparameter.h"
#include "parameters/timeparameter.h"
#include "effects/effectfactory.h"

using namespace perspective;

Perspective* g_perspective = nullptr;

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    if (g_perspective) {
        g_perspective->AudioCallbackImpl(in, out, size);
    }
}



Perspective::Perspective() 
    : currentEffect_(nullptr) {
    g_perspective = this;

    // Create settings parameters (unified model)
    // Tuning reference (EncoderParameter, matches TunerEffect)
    settingsParameters_.push_back(new EncoderParameter("E1 Tuner Ref", 420.0f, 460.0f, 440.0f, 0.5f, ENCODER_1_IDX, 2));
    // Metronome volume (PotentiometerParameter, log taper)
    settingsParameters_.push_back(new PotentiometerParameter("K1 Met Vol", 0.0f, 1.0f, 0.7f, PotCurve::LOG, KNOB_1_IDX, 0));

    // Metronome mode (PotentiometerParameter, discrete, knob 3)
    static const char* kMetronomeModes[] = {"Bass", "Snare", "High", "Click"};
    auto* modeParam = new PotentiometerParameter("K2 Met Mode", 0.0f, 3.0f, 0.0f, PotCurve::LIN, KNOB_2_IDX, 1);
    modeParam->SetDisplayType(DisplayType::DISCRETE);
    modeParam->SetDiscreteValues(kMetronomeModes, 4);
    settingsParameters_.push_back(modeParam);

    // Bypass type (EncoderParameter, encoder 2)
    static const char* kBypassTypeLabels[] = {"Pass", "True"};
    auto* bypassParam = new EncoderParameter("E2 Bypass", 0.0f, 1.0f, 0.0f, 1.0f, ENCODER_2_IDX, 3);
    bypassParam->SetDisplayType(DisplayType::DISCRETE);
    bypassParam->SetDiscreteValues(kBypassTypeLabels, 2);
    settingsParameters_.push_back(bypassParam);
}

Perspective::~Perspective() {

}

void Perspective::Init() {
    hardware.Init(GetEventHandler());

    LoadEffects(); // Load effects before registering listeners so we can populate effect selection menu

    LoadPresetsFromFlash();

    // Initialize perspective-specific UI elements
    RegisterEventListeners();

    hardware.StartAudio(AudioCallback);
}

void Perspective::Exec() {
    while(true) {
        if (mode_ == PerspectiveMode::TUNER) {
            // In tuner mode skip control/event processing - only sw1 raw poll needed for exit.
            perspective::Switch* sw1 = hardware.GetSwitch(0);
            if (sw1) {
                bool pressed = sw1->RawState();
                if (!tunerSw1ReleasedOnce_) {
                    if (!pressed) tunerSw1ReleasedOnce_ = true;
                } else if (pressed) {
                    ExitTunerMode();
                }
            }

            uint32_t now = System::GetNow();
            if (now - lastTunerDisplayTime_ >= 100) {
                lastTunerDisplayTime_ = now;
                UpdateTunerDisplay();
            }
        } else {
            hardware.ProcessControls();
            eventHandler_.ProcessEvents();
        }

        // Service deferred flash save outside the audio ISR to avoid a glitch.
        // switchingEffect_ mutes the audio callback for the duration of the write.
        if (pendingFlashSave_) {
            pendingFlashSave_ = false;
            switchingEffect_ = true;
            SavePresetsToFlash();
            switchingEffect_ = false;
        }

        if (volumeMode_) {
            UpdateVolumeLevel();
        }

        hardware.DelayMs(1); // Small delay to allow events to accumulate
    }
}     
void Perspective::AudioCallbackImpl(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    float ledPulseBrightness = 0.0f;

    if (switchingEffect_) {
        for (size_t i = 0; i < size; i++) {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    } else if (mode_ == PerspectiveMode::TUNER && tunerEffect_) {
        // Tuner mode: process for pitch detection but mute output
        tunerEffect_->ProcessStereo(in[0], in[1], out[0], out[1], size);
        for (size_t i = 0; i < size; i++) {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    } else if (mode_ == PerspectiveMode::PRESET && presetMuted_) {
        // Preset mode with empty slot: mute output
        for (size_t i = 0; i < size; i++) {
            out[0][i] = 0.0f;
            out[1][i] = 0.0f;
        }
    } else if (currentEffect_ && !bypassMode_) {
        // Process with current effect
        currentEffect_->ProcessStereo(in[0], in[1], out[0], out[1], size);
        ledPulseBrightness = currentEffect_->GetTempoPulseBrightness();
        hardware.SetLedBrightness(LED_2_IDX, currentEffect_->GetEnvelopeBrightness());
    } else {
        // Bypass or fallback: pass input through unchanged
        for (size_t i = 0; i < size; i++){
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
    }

    if (volumeMode_) {
        float vol = volumeLevel_;
        for (size_t i = 0; i < size; i++) {
            out[0][i] *= vol;
            out[1][i] *= vol;
        }
    }

    hardware.SetLedBrightness(LED_1_IDX, ledPulseBrightness);
}

void Perspective::RegisterEventListeners() {
    // Register event listeners, setup display, etc.
    
    // Generic listener for knob changes - updates effect parameters
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT) return;
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
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
            if (!currentEffect_) return;
            if (mode_ == PerspectiveMode::PRESET && presetEditMode_) return;
            // In preset mode, only allow encoder 1 changes for tempo effects
            if (mode_ == PerspectiveMode::PRESET
                && (!currentEffect_->HasTempoMode() || event.controlIndex != ENCODER_1_IDX)) return;

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


    // Settings mode: handle all settings parameters (knob/encoder)
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::SETTINGS) return;
            for (size_t i = 0; i < settingsParameters_.size(); i++) {
                EffectParameter* param = settingsParameters_[i];
                if (param && param->GetIndex() == event.controlIndex) {
                    if (param->GetType() == ParameterType::ENCODER) {
                        EncoderParameter* encParam = static_cast<EncoderParameter*>(param);
                        if (event.value > 0) {
                            encParam->Increment(event.value);
                        } else if (event.value < 0) {
                            encParam->Decrement(-event.value);
                        }
                    } else if (param->GetType() == ParameterType::POTENTIOMETER) {
                        PotentiometerParameter* potParam = static_cast<PotentiometerParameter*>(param);
                        Knob* knob = static_cast<Knob*>(event.source);
                        float normalizedValue = knob->Value();
                        potParam->SetNormalizedValueWithCurve(normalizedValue);
                    }
                    // Update display
                    UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
                }
            }
        },
        UIEventType::ENCODER_CHANGED
    );
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::SETTINGS) return;
            for (size_t i = 0; i < settingsParameters_.size(); i++) {
                EffectParameter* param = settingsParameters_[i];
                if (param && param->GetIndex() == event.controlIndex && param->GetType() == ParameterType::POTENTIOMETER) {
                    PotentiometerParameter* potParam = static_cast<PotentiometerParameter*>(param);
                    Knob* knob = static_cast<Knob*>(event.source);
                    float normalizedValue = knob->Value();
                    potParam->SetNormalizedValueWithCurve(normalizedValue);
                    UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
                }
            }
        },
        UIEventType::KNOB_CHANGED
    );
    
    // Generic listener for button presses - updates toggle parameters
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
            if (!currentEffect_) return;
            if (mode_ == PerspectiveMode::PRESET && presetEditMode_) return;
            // In preset mode, only allow encoder 1 button toggles for tempo effects
            if (mode_ == PerspectiveMode::PRESET
                && (!currentEffect_->HasTempoMode() || event.controlIndex != ENCODER_1_BUTTON_IDX)) return;
            
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
                        
                        // Update effect with new parameter value
                        currentEffect_->Update();
                        break;
                    }
               }
            }
        },
        UIEventType::BUTTON_RELEASED
    );
    
    // Register listener for Switch_1 being pressed (next effect / exit settings / preset up)
    // Note: tuner exit is handled via direct polling in Exec() to bypass event timing issues
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ == PerspectiveMode::SETTINGS) {
                ExitSettingsMode();
                return;
            }
            if (mode_ == PerspectiveMode::PRESET) {
                if (presetEditMode_) return;
                currentPresetSlot_ = (currentPresetSlot_ + 1) % PRESET_COUNT;
                LoadPresetAtIndex(currentPresetSlot_);
                return;
            }
            if (mode_ != PerspectiveMode::EFFECT) return;
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

    // Preset mode: encoder 2 rotation selects edit option (only when in edit sub-mode)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::PRESET) return;
            if (!presetEditMode_) return;
            
            int optionCount = static_cast<int>(PresetEditOption::COUNT);
            if (event.value > 0) {
                presetEditSelection_ = (presetEditSelection_ + 1) % optionCount;
            } else if (event.value < 0) {
                presetEditSelection_ = (presetEditSelection_ + optionCount - 1) % optionCount;
            }
            UpdatePresetDisplay();
        },
        UIEventType::ENCODER_CHANGED,
        1  // Index 1 = Encoder 2
    );

    // Preset mode: encoder 2 button toggles edit sub-mode / confirms action
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::PRESET) return;
            
            if (!presetEditMode_) {
                EnterPresetEditMode();
            } else {
                ExecutePresetEditAction();
            }
        },
        UIEventType::BUTTON_RELEASED,
        ENCODER_2_BUTTON_IDX
    );
    
    // Register listener for Switch_1 being held (enter preset mode / exit preset mode)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ == PerspectiveMode::PRESET) {
                if (presetEditMode_) return;
                ExitPresetMode();
                return;
            }
            if (mode_ == PerspectiveMode::EFFECT) {
                EnterPresetMode();
            }
        },
        UIEventType::BUTTON_HELD,
        0  // Index 0 = Switch_1
    );

    // Register listener for Switch_1 + Switch_2 held together (enter settings mode)
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT) return;
            bool isSwitch1And2 = (event.controlIndex == 0 && event.previousValue == 1)
                              || (event.controlIndex == 1 && event.previousValue == 0);
            if (isSwitch1And2) {
                EnterSettingsMode();
            }
        },
        UIEventType::BUTTONS_HELD_TOGETHER
    );
    
    // Register listener for Switch_2 being pressed (previous effect / preset down)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ == PerspectiveMode::PRESET) {
                if (presetEditMode_) return;
                currentPresetSlot_ = (currentPresetSlot_ + PRESET_COUNT - 1) % PRESET_COUNT;
                LoadPresetAtIndex(currentPresetSlot_);
                return;
            }
            if (mode_ != PerspectiveMode::EFFECT) return;
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

    // Register listener for Switch_2 being held (enter tuner mode)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ == PerspectiveMode::EFFECT) {
                EnterTunerMode();
            }
        },
        UIEventType::BUTTON_HELD,
        1  // Index 1 = Switch_2
    );
    
    // Register listener for Switch_3 being pressed (bypass toggle) - works in EFFECT and PRESET modes
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
            ToggleBypass();
        },
        UIEventType::BUTTON_RELEASED,
        2  // Index 2 = Switch_3
    );
    
    // Register listener for Switch_3 being held (enter/exit volume mode)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
            if (volumeMode_) {
                ExitVolumeMode();
            } else if (CanEnterVolumeMode()) {
                EnterVolumeMode();
            }
        },
        UIEventType::BUTTON_HELD,
        2  // Index 2 = Switch_3
    );
    
    // Register listener for Switch_4 being pressed (tap tempo) - works in EFFECT and PRESET modes
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
            HandleTapTempo();
        },
        UIEventType::BUTTON_RELEASED,
        3  // Index 3 = Switch_4
    );
    
    // Register listener for Switch_4 being held (metronome toggle) - works in EFFECT and PRESET modes
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT && mode_ != PerspectiveMode::PRESET) return;
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
    currentEffect_->SetMetronomeLevel(metronomeLevel_);
    currentEffect_->Update();
    
    UpdateStatusDisplay();
}

void Perspective::EnterVolumeMode() {
    volumeMode_ = true;
    UpdateStatusDisplay();
}

void Perspective::ExitVolumeMode() {
    volumeMode_ = false;
    volumeLevel_ = 1.0f;
    UpdateStatusDisplay();
}

void Perspective::UpdateVolumeLevel() {
    Knob* expKnob = hardware.GetKnob(KNOB_EXP_IDX);
    if (!expKnob) return;
    expKnob->Process(); // refresh raw_ from ADC
    expKnob->Filter();  // apply smoothing, update raw_val_
    float v = expKnob->Value(); // calibrated 0-1
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    volumeLevel_ = taperFunction(v, 0.12f); // LOG taper (ym=0.12)
}

bool Perspective::CanEnterVolumeMode() const {
    return hardware.IsJackExpressionInserted()
        && currentEffect_
        && !currentEffect_->UsesExpressionPedal();
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
            if (switchingEffect_) return; // Suppress during effect switching
            if (mode_ == PerspectiveMode::PRESET) {
                // In preset mode, only encoder 1 params on tempo effects are highlighted
                bool controllable = currentEffect_ && currentEffect_->HasTempoMode()
                    && param->GetType() == ParameterType::ENCODER
                    && param->GetIndex() == ENCODER_1_IDX;
                if (controllable) {
                    this->UpdateParameterDisplayHighlighted(param, displayIndex + 1);
                } else {
                    this->UpdateParameterDisplay(param, displayIndex + 1);
                }
            } else {
                this->UpdateParameterDisplay(param, displayIndex + 1); // +1 to skip effect name display
            }
        });
    }
    
    // Set the first effect as current
    SetCurrentEffect(0);
}

void Perspective::SetCurrentEffect(size_t index) {
    if (index >= effects_.size()) {
        return; // Invalid index
    }

    switchingEffect_ = true;

    Effect* nextEffect = effects_[index];

    if (currentEffect_) {
        currentEffect_->OnDeselected();
    }

    nextEffect->OnSelected();
    
    // Propagate global metronome state to new effect (propagates through compound effect hierarchy)
    nextEffect->SetMetronomeEnabled(metronomeEnabled_);
    nextEffect->SetMetronomeLevel(metronomeLevel_);
    
    // Update effect to apply metronome state propagation through compound effect children
    nextEffect->Update();

    currentEffectIndex_ = index;
    currentEffect_ = nextEffect;
    switchingEffect_ = false;

    // Auto-exit volume mode if the new effect uses the expression pedal
    if (volumeMode_ && currentEffect_->UsesExpressionPedal()) {
        ExitVolumeMode();
    }

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

void Perspective::UpdateParameterDisplayHighlighted(EffectParameter* param, size_t displayIndex) {
    if (!param) return;
    
    char valueStr[16];
    param->GetValueAsString(valueStr, sizeof(valueStr));
    hardware.SetParameterDisplayHighlighted(displayIndex, param->GetName(), valueStr);
}

void Perspective::UpdateStatusDisplay() {
    const char* bypassText = bypassMode_ ? "BP" : "";
    const char* volText    = volumeMode_ ? "Vol" : "";
    const char* metroText  = metronomeEnabled_ ? "Met" : "";
    hardware.SetStatusDisplay(bypassText, volText, metroText);
}

void Perspective::EnterTunerMode() {
    if (!tunerEffect_) return;
    
    mode_ = PerspectiveMode::TUNER;
    bypassMode_ = false;  // Disable bypass when entering tuner mode

    // Init direct-poll state for exit detection
    tunerSw1ReleasedOnce_ = false;
    lastTunerDisplayTime_ = 0;  // Force immediate first update
    
    // Clear parameter text and enable full-screen tuner overlay
    hardware.ClearDisplay();
    
    // Initial tuner display
    UpdateTunerDisplay();
}

void Perspective::ExitTunerMode() {
    mode_ = PerspectiveMode::EFFECT;
    hardware.HideTunerOverlay();
    
    // Restore current effect display
    SetCurrentEffect(currentEffectIndex_);
}

void Perspective::EnterSettingsMode() {
    mode_ = PerspectiveMode::SETTINGS;

    // Sync bypass type parameter to current runtime state before showing it
    if (settingsParameters_.size() > kSettingsParamBypassType) {
        settingsParameters_[kSettingsParamBypassType]->SetValue(
            bypassType_ == BypassType::TRUE_BYPASS ? 1.0f : 0.0f);
    }

    hardware.ClearDisplay();
    hardware.SetParameterDisplay(0, "Settings", "");
    // Show all settings parameters
    for (size_t i = 0; i < settingsParameters_.size(); i++) {
        EffectParameter* param = settingsParameters_[i];
        if (param && param->GetDisplayIndex() >= 0) {
            UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
        }
    }
}

void Perspective::ExitSettingsMode() {
    // On exit, propagate settings parameter values to global state
    // Tuning reference
    if (settingsParameters_.size() > kSettingsParamTuningReference) {
        float ref = settingsParameters_[kSettingsParamTuningReference]->GetValue();
        if (tunerEffect_ && tunerEffect_->GetParameterCount() > 0) {
            tunerEffect_->GetParameter(0)->SetValue(ref);
            tunerEffect_->Update();
        }
    }
    // Metronome level
    if (settingsParameters_.size() > kSettingsParamMetronomeLevel) {
        metronomeLevel_ = settingsParameters_[kSettingsParamMetronomeLevel]->GetValue();
        if (currentEffect_) {
            currentEffect_->SetMetronomeLevel(metronomeLevel_);
        }
    }
    // Metronome mode
    if (settingsParameters_.size() > kSettingsParamMetronomeMode) {
        metronomeMode_ = settingsParameters_[kSettingsParamMetronomeMode]->GetValueAsInt(3);
    }
    // Bypass type
    if (settingsParameters_.size() > kSettingsParamBypassType) {
        int val = settingsParameters_[kSettingsParamBypassType]->GetValueAsInt(1);
        bypassType_ = (val == 1) ? BypassType::TRUE_BYPASS : BypassType::PASSTHROUGH;
        // Re-apply relay state with new bypass type
        if (bypassMode_ && bypassType_ == BypassType::TRUE_BYPASS) {
            hardware.SetTrueBypass(true);
        } else {
            hardware.SetTrueBypass(false);
        }
    }
    mode_ = PerspectiveMode::EFFECT;
    SetCurrentEffect(currentEffectIndex_);
}

void Perspective::EnterPresetMode() {
    CacheCurrentEffect();
    mode_ = PerspectiveMode::PRESET;
    presetEditMode_ = false;
    presetEditSelection_ = 0;
    LoadPresetAtIndex(currentPresetSlot_);
}

void Perspective::ExitPresetMode() {
    presetEditMode_ = false;
    mode_ = PerspectiveMode::EFFECT;
    RestoreCachedEffect();
}

void Perspective::CacheCurrentEffect() {
    cachedEffectIndex_ = currentEffectIndex_;
    cachedParamCount_ = 0;
    if (currentEffect_) {
        size_t count = currentEffect_->GetParameterCount();
        if (count > PRESET_MAX_PARAMS) count = PRESET_MAX_PARAMS;
        for (size_t i = 0; i < count; i++) {
            EffectParameter* p = currentEffect_->GetParameter(i);
            cachedParamValues_[i] = p ? p->GetValue() : 0.0f;
        }
        cachedParamCount_ = count;
    }
}

void Perspective::RestoreCachedEffect() {
    if (cachedEffectIndex_ >= effects_.size()) {
        SetCurrentEffect(0);
        return;
    }
    
    // Switch to the cached effect
    SetCurrentEffect(cachedEffectIndex_);
    
    // Restore cached parameter values
    if (currentEffect_) {
        size_t count = currentEffect_->GetParameterCount();
        if (count > cachedParamCount_) count = cachedParamCount_;
        for (size_t i = 0; i < count; i++) {
            EffectParameter* p = currentEffect_->GetParameter(i);
            if (p) p->SetValue(cachedParamValues_[i]);
        }
        currentEffect_->Update();
        
        // Refresh display with restored values
        hardware.ClearDisplay();
        hardware.SetParameterDisplay(0, currentEffect_->GetName(), "");
        for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
            EffectParameter* param = currentEffect_->GetParameter(i);
            if (param && param->GetDisplayIndex() >= 0) {
                UpdateParameterDisplay(param, param->GetDisplayIndex() + 1);
            }
        }
        UpdateStatusDisplay();
    }
}

void Perspective::LoadPresetAtIndex(size_t slot) {
    if (slot >= PRESET_COUNT) return;
    
    presetMuted_ = !presetBank_.IsOccupied(slot);
    
    if (presetMuted_) {
        // Empty slot - show empty label, mute output
        hardware.ClearDisplay();
        char title[32];
        snprintf(title, sizeof(title), "P%d ** Empty **", static_cast<int>(slot + 1));
        hardware.SetParameterDisplay(0, title, "");
        UpdateStatusDisplay();
        return;
    }
    
    const PresetData& preset = presetBank_.Get(slot);
    
    if (preset.effectIndex >= effects_.size()) {
        presetMuted_ = true;
        hardware.ClearDisplay();
        char title[32];
        snprintf(title, sizeof(title), "P%d ?? Invalid ??", static_cast<int>(slot + 1));
        hardware.SetParameterDisplay(0, title, "");
        return;
    }
    
    // Switch to the preset's effect
    switchingEffect_ = true;
    
    Effect* presetEffect = effects_[preset.effectIndex];
    if (currentEffect_) {
        currentEffect_->OnDeselected();
    }
    presetEffect->OnSelected();
    
    // Restore parameter values from preset
    size_t count = presetEffect->GetParameterCount();
    if (count > preset.paramCount) count = preset.paramCount;
    for (size_t i = 0; i < count; i++) {
        EffectParameter* p = presetEffect->GetParameter(i);
        if (p) p->SetValue(preset.params[i].value);
    }
    
    presetEffect->SetMetronomeEnabled(metronomeEnabled_);
    presetEffect->SetMetronomeLevel(metronomeLevel_);
    presetEffect->Update();
    
    currentEffectIndex_ = preset.effectIndex;
    currentEffect_ = presetEffect;
    switchingEffect_ = false;

    // Auto-exit volume mode if the preset effect uses the expression pedal
    if (volumeMode_ && currentEffect_->UsesExpressionPedal()) {
        ExitVolumeMode();
    }

    UpdatePresetDisplay();
}

void Perspective::UpdatePresetDisplay() {
    hardware.ClearDisplay();
    
    char title[32];
    if (presetBank_.IsOccupied(currentPresetSlot_)) {
        const PresetData& preset = presetBank_.Get(currentPresetSlot_);
        snprintf(title, sizeof(title), "P%d %s", static_cast<int>(currentPresetSlot_ + 1), preset.name);
    } else {
        snprintf(title, sizeof(title), "P%d ** Empty **", static_cast<int>(currentPresetSlot_ + 1));
    }
    hardware.SetParameterDisplay(0, title, "");
    
    // Show all parameters; grey out those that can't be controlled in preset mode
    if (!presetMuted_ && currentEffect_) {
        bool hasTempo = currentEffect_->HasTempoMode();
        for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
            EffectParameter* param = currentEffect_->GetParameter(i);
            if (param && param->GetDisplayIndex() >= 0) {
                size_t slot = param->GetDisplayIndex() + 1;
                bool controllable = hasTempo && param->GetType() == ParameterType::ENCODER
                    && param->GetIndex() == ENCODER_1_IDX;
                if (controllable) {
                    UpdateParameterDisplayHighlighted(param, slot);
                } else {
                    UpdateParameterDisplay(param, slot);
                }
            }
        }
    }
    
    // Show edit options if in edit sub-mode
    if (presetEditMode_) {
        static const char* editOptionNames[] = { "Save", "Overwrite", "Delete", "Cancel" };
        hardware.SetParameterDisplay(8, "Edit:", editOptionNames[presetEditSelection_]);
    }
    
    UpdateStatusDisplay();
}

void Perspective::EnterPresetEditMode() {
    presetEditMode_ = true;
    presetEditSelection_ = 0;
    UpdatePresetDisplay();
}

void Perspective::ExitPresetEditMode() {
    presetEditMode_ = false;
    UpdatePresetDisplay();
}

void Perspective::ExecutePresetEditAction() {
    PresetEditOption action = static_cast<PresetEditOption>(presetEditSelection_);
    
    switch (action) {
        case PresetEditOption::SAVE: {
            if (!presetBank_.IsOccupied(currentPresetSlot_)) {
                presetBank_.Save(currentPresetSlot_, cachedEffectIndex_,
                    effects_[cachedEffectIndex_]->GetName(), cachedParamValues_, cachedParamCount_);
                pendingFlashSave_ = true;
                LoadPresetAtIndex(currentPresetSlot_);
            }
            break;
        }
        case PresetEditOption::OVERWRITE: {
            presetBank_.Save(currentPresetSlot_, cachedEffectIndex_,
                effects_[cachedEffectIndex_]->GetName(), cachedParamValues_, cachedParamCount_);
            pendingFlashSave_ = true;
            LoadPresetAtIndex(currentPresetSlot_);
            break;
        }
        case PresetEditOption::DELETE: {
            presetBank_.ClearSlot(currentPresetSlot_);
            pendingFlashSave_ = true;
            LoadPresetAtIndex(currentPresetSlot_);
            break;
        }
        case PresetEditOption::CANCEL:
        default:
            break;
    }
    
    ExitPresetEditMode();
}

void Perspective::UpdateTunerDisplay() {
    if (!tunerEffect_ || mode_ != PerspectiveMode::TUNER) return;

    hardware.ShowTunerOverlay(
        tunerEffect_->GetNoteName(),
        tunerEffect_->GetNoteOctave(),
        tunerEffect_->GetCentsOffset(),
        tunerEffect_->GetDetectedFrequency(),
        tunerEffect_->GetTuningReference(),
        tunerEffect_->IsSignalDetected()
    );
}

// ---------------------------------------------------------------------------
// Preset flash persistence
//
// Storage layout (one 4 KB sector at the end of the 8 MB IS25LP064A):
//   Offset 0x7FF000 : uint32_t magic   (0x50525354 = "PRST")
//   Offset 0x7FF004 : uint32_t version (currently 1)
//   Offset 0x7FF008 : PresetData[PRESET_COUNT]
//
// The QSPI peripheral is initialised in MEMORY_MAPPED mode by DaisySeed::Init().
// Writing requires a temporary switch to INDIRECT_POLLING mode, then back.
// Running from BOOT_SRAM means we are never executing from QSPI, so the mode
// switch is safe at any time (outside the audio ISR).
// ---------------------------------------------------------------------------

static constexpr uint32_t kPresetFlashOffset = 0x7FF000; // last 4 KB of 8 MB IS25LP064A
static constexpr uint32_t kPresetMagic       = 0x50525354; // "PRST"
static constexpr uint32_t kPresetVersion     = 1;

struct PresetFlashHeader {
    uint32_t magic;
    uint32_t version;
};

void Perspective::LoadPresetsFromFlash() {
    // In MEMORY_MAPPED mode the flash contents are directly readable via GetData().
    const auto* hdr = static_cast<const PresetFlashHeader*>(hardware.qspi.GetData(kPresetFlashOffset));

    if (hdr->magic != kPresetMagic || hdr->version != kPresetVersion) {
        // First boot or layout change – start with a blank bank.
        presetBank_.Clear();
        Hardware::PrintLine("Presets: no valid flash data, starting empty.");
        return;
    }

    const auto* src = reinterpret_cast<const PresetData*>(hdr + 1);
    for (size_t i = 0; i < PRESET_COUNT; ++i) {
        if (src[i].occupied) {
            presetBank_.Save(i, src[i].effectIndex, src[i].name,
                             &src[i].params[0].value, src[i].paramCount);
        } else {
            presetBank_.ClearSlot(i);
        }
    }
    Hardware::PrintLine("Presets: loaded from flash.");
}

void Perspective::SavePresetsToFlash() {
    // Build the sector image in a static buffer to avoid a large stack allocation.
    static constexpr size_t kBufSize = sizeof(PresetFlashHeader) + sizeof(PresetData) * PRESET_COUNT;
    static uint8_t buf[kBufSize];

    auto* hdr = reinterpret_cast<PresetFlashHeader*>(buf);
    hdr->magic   = kPresetMagic;
    hdr->version = kPresetVersion;

    auto* dst = reinterpret_cast<PresetData*>(hdr + 1);
    for (size_t i = 0; i < PRESET_COUNT; ++i) {
        dst[i] = presetBank_.Get(i);
    }

    // Switch QSPI to indirect-polling mode for erase/write.
    hardware.qspi.DeInit();
    hardware.qspi_config.mode = QSPIHandle::Config::Mode::INDIRECT_POLLING;
    hardware.qspi.Init(hardware.qspi_config);

    hardware.qspi.EraseSector(kPresetFlashOffset);
    hardware.qspi.Write(kPresetFlashOffset, kBufSize, buf);

    // Restore memory-mapped mode.
    hardware.qspi.DeInit();
    hardware.qspi_config.mode = QSPIHandle::Config::Mode::MEMORY_MAPPED;
    hardware.qspi.Init(hardware.qspi_config);

    Hardware::PrintLine("Presets: saved to flash.");
}
