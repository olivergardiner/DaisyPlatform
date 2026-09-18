#include "perspective.h"
#include "effects/effect.h"
#include "parameters/effectparameter.h"
#include "parameters/potentiometerparameter.h"
#include "parameters/encoderparameter.h"
#include "parameters/toggleparameter.h"
#include "parameters/timeparameter.h"
#include "effects/effectfactory.h"
#include <cmath>
#include <cstring>

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

#if defined(PERSPECTIVE_PLATFORM_AMP)
    // Cab sim controls. The cab is permanently in circuit on channel 2, so
    // these are global settings rather than per-effect parameters. Low cut and
    // cone resonance are left at the cab's own defaults; these three are the
    // ones worth reaching for when matching a desk or a power amp.
    settingsParameters_.push_back(new PotentiometerParameter("K3 Cab Roll", 2500.0f, 7000.0f, 4200.0f, PotCurve::LOG, KNOB_3_IDX, 4));
    settingsParameters_.push_back(new PotentiometerParameter("K4 Cab Pres", -6.0f, 9.0f, 4.0f, PotCurve::LIN, KNOB_4_IDX, 5));
    settingsParameters_.push_back(new PotentiometerParameter("K5 Cab Vol", -12.0f, 12.0f, 0.0f, PotCurve::LIN, KNOB_5_IDX, 6));
#endif
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
        // Tuner mode: process for pitch detection but mute output.
        // Mono on both platforms — the output is zeroed regardless, and the
        // stereo path would feed the detector channel 0 then channel 1 through
        // the same state, so on a mono guitar rig it saw signal interleaved
        // with silence.
        tunerEffect_->Process(in[0], out[0], size);
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
        // Hand the dry input down as a sidechain key before processing, so
        // detector-driven effects can follow the guitar rather than their own input.
        currentEffect_->SetKeyInput(in[0], size);
#if defined(PERSPECTIVE_PLATFORM_AMP)
        // Mono FX on channel 1, the same signal through the cab sim on
        // channel 2 — one output for a real amp, one for a desk or interface.
        currentEffect_->Process(in[0], out[0], size);
        if (cabSimEffect_) {
            cabSimEffect_->Process(out[0], out[1], size);
        } else {
            for (size_t i = 0; i < size; i++) out[1][i] = out[0][i];
        }
#else
        currentEffect_->ProcessStereo(in[0], in[1], out[0], out[1], size);
#endif
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

                        // Macro knobs (Mix/Depth/Rate/Feedback) require the physical pot to be
                        // moved within kMacroKnobCatchThreshold of the stored value before it
                        // takes control, avoiding value jumps on effect/preset change.
                        int macroSlot = MacroKnobSlotForControlIndex(event.controlIndex);
                        if (macroSlot >= 0 && param->GetMacroRole() != MacroRole::NONE && param->IsMacroPrimary()) {
                            if (!macroKnobCaught_[macroSlot]) {
                                float target = param->GetNormalizedValue();
                                if (fabsf(normalizedValue - target) > kMacroKnobCatchThreshold) {
                                    break; // Not caught yet - ignore this movement
                                }
                                macroKnobCaught_[macroSlot] = true;
                            }
                        }

                        potParam->SetNormalizedValueWithCurve(normalizedValue);
                        
                        // Update display (only if not hidden)
                        if (param->GetDisplayIndex() >= 0) {
                            UpdateParameterRow(param, param->GetDisplayIndex() + 1);
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

    // Effect mode: Encoder 1 rotate = select parameter, Encoder 2 rotate = set value of selected parameter (while editing)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT) return;
            if (!currentEffect_) return;
            if (paramEditMode_) return; // Selection locked while editing
            if (event.value > 0) {
                SelectAdjacentParameter(1);
            } else if (event.value < 0) {
                SelectAdjacentParameter(-1);
            }
        },
        UIEventType::ENCODER_CHANGED,
        ENCODER_1_IDX
    );
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::EFFECT) return;
            if (!currentEffect_) return;
            if (!paramEditMode_) return; // Rotation only adjusts value while editing
            AdjustSelectedParameter(event.value);
        },
        UIEventType::ENCODER_CHANGED,
        ENCODER_2_IDX
    );

    // Preset mode: encoder changes only pass through for the tempo effect's Encoder 1 (unchanged behavior)
    eventHandler_.RegisterListener(
        [this](const UIEvent& event) {
            if (mode_ != PerspectiveMode::PRESET) return;
            if (!currentEffect_) return;
            if (presetEditMode_) return;
            // In preset mode, only allow encoder 1 changes for tempo effects
            if (!currentEffect_->HasTempoMode() || event.controlIndex != ENCODER_1_IDX) return;

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

                        // Tempo Mode toggles only fire while their paired Time parameter is selected in edit mode
                        if (mode_ == PerspectiveMode::EFFECT && strcmp(param->GetName(), "Tempo Mode") == 0
                            && !IsSelectedParameterPairedTempoTime(param)) {
                            break;
                        }

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
    // Effect mode: encoder 2 button toggles parameter edit mode
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            if (mode_ == PerspectiveMode::PRESET) {
                if (!presetEditMode_) {
                    EnterPresetEditMode();
                } else {
                    ExecutePresetEditAction();
                }
                return;
            }
            if (mode_ == PerspectiveMode::EFFECT) {
                ToggleParameterEditMode();
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

#if defined(PERSPECTIVE_PLATFORM_AMP)
    // Likewise the cab sim: a platform fixture on channel 2, not selectable
    cabSimEffect_ = new CabSimEffect();
    cabSimEffect_->Init(sampleRate);
    ApplyCabSettings();
#endif
    
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
            } else if (mode_ == PerspectiveMode::EFFECT && (int)displayIndex == param->GetDisplayIndex()
                       && selectedParamIndex_ >= 0 && currentEffect_
                       && currentEffect_->GetParameter(selectedParamIndex_) == param) {
                if (paramEditMode_) {
                    this->UpdateParameterDisplayEditing(param, displayIndex + 1);
                } else {
                    this->UpdateParameterDisplayHighlighted(param, displayIndex + 1);
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

    ArmMacroKnobCatch();
    ResetParameterSelection();

    // Auto-exit volume mode if the new effect uses the expression pedal
    if (volumeMode_ && currentEffect_->UsesExpressionPedal()) {
        ExitVolumeMode();
    }

    // Clear the display before showing new effect
    hardware.ClearDisplay();
    
    hardware.SetParameterDisplay(0, currentEffect_->GetName(), "");
    
    // Display initial values for visible parameters only, highlighting the selected one
    RefreshParameterDisplays();
    
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

void Perspective::UpdateParameterDisplayEditing(EffectParameter* param, size_t displayIndex) {
    if (!param) return;
    
    char valueStr[16];
    param->GetValueAsString(valueStr, sizeof(valueStr));
    hardware.SetParameterDisplayEditing(displayIndex, param->GetName(), valueStr);
}

void Perspective::UpdateParameterRow(EffectParameter* param, size_t displayIndex) {
    if (!param) return;

    // Preserve the select/edit highlight when a macro pot or other control updates a
    // row that happens to be the parameter currently selected via the encoders.
    bool isSelected = mode_ == PerspectiveMode::EFFECT && selectedParamIndex_ >= 0
        && currentEffect_ && currentEffect_->GetParameter(static_cast<size_t>(selectedParamIndex_)) == param;

    if (isSelected && paramEditMode_) {
        UpdateParameterDisplayEditing(param, displayIndex);
    } else if (isSelected) {
        UpdateParameterDisplayHighlighted(param, displayIndex);
    } else {
        UpdateParameterDisplay(param, displayIndex);
    }
}

void Perspective::UpdateStatusDisplay() {
    const char* bypassText = bypassMode_ ? "BP" : "";
    const char* volText    = volumeMode_ ? "Vol" : "";
    const char* metroText  = metronomeEnabled_ ? "Met" : "";
    hardware.SetStatusDisplay(bypassText, volText, metroText);
}

int Perspective::MacroKnobSlotForControlIndex(int controlIndex) {
    switch (controlIndex) {
        case MACRO_KNOB_MIX_IDX:         return 0;
        case MACRO_KNOB_DEPTH_IDX:       return 1;
        case MACRO_KNOB_RATE_IDX:        return 2;
        case MACRO_KNOB_FEEDBACK_IDX:    return 3;
        case MACRO_KNOB_SUBDIVISION_IDX: return 4;
        default: return -1;
    }
}

void Perspective::ArmMacroKnobCatch() {
    for (int i = 0; i < NUM_MACRO_KNOBS; i++) {
        macroKnobCaught_[i] = false;
    }
}

void Perspective::ResetParameterSelection() {
    selectedParamIndex_ = -1;
    paramEditMode_ = false;
    if (!currentEffect_) return;
    // Auto-select the first visible parameter
    for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
        EffectParameter* param = currentEffect_->GetParameter(i);
        if (param && param->GetDisplayIndex() >= 0) {
            selectedParamIndex_ = static_cast<int>(i);
            break;
        }
    }
}

void Perspective::SelectAdjacentParameter(int direction) {
    if (!currentEffect_) return;
    size_t count = currentEffect_->GetParameterCount();
    if (count == 0) return;

    int start = selectedParamIndex_;
    int idx = start;
    for (size_t step = 0; step < count; step++) {
        idx = static_cast<int>((idx + direction + static_cast<int>(count)) % static_cast<int>(count));
        EffectParameter* param = currentEffect_->GetParameter(static_cast<size_t>(idx));
        if (param && param->GetDisplayIndex() >= 0) {
            selectedParamIndex_ = idx;
            break;
        }
    }

    RefreshParameterDisplays();
}

void Perspective::ToggleParameterEditMode() {
    if (!currentEffect_) return;
    if (selectedParamIndex_ < 0) return;
    paramEditMode_ = !paramEditMode_;
    RefreshParameterDisplays();
}

// Tempo Mode toggles are paired with the Nth TimeParameter by declaration order within the effect
bool Perspective::IsSelectedParameterPairedTempoTime(EffectParameter* tempoModeToggle) const {
    if (!currentEffect_ || !tempoModeToggle || selectedParamIndex_ < 0) return false;
    EffectParameter* selected = currentEffect_->GetParameter(static_cast<size_t>(selectedParamIndex_));
    if (!selected) return false;

    int toggleOrdinal = -1;
    int timeOrdinal = -1;
    int toggleCount = 0;
    int timeCount = 0;
    for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
        EffectParameter* p = currentEffect_->GetParameter(i);
        if (!p) continue;
        if (p->GetType() == ParameterType::TOGGLE && strcmp(p->GetName(), "Tempo Mode") == 0) {
            if (p == tempoModeToggle) toggleOrdinal = toggleCount;
            toggleCount++;
        } else if (p->IsTimeParameter()) {
            if (p == selected) timeOrdinal = timeCount;
            timeCount++;
        }
    }
    return toggleOrdinal >= 0 && toggleOrdinal == timeOrdinal;
}

void Perspective::AdjustSelectedParameter(int steps) {
    if (!currentEffect_ || selectedParamIndex_ < 0 || steps == 0) return;
    EffectParameter* param = currentEffect_->GetParameter(static_cast<size_t>(selectedParamIndex_));
    if (!param) return;

    switch (param->GetType()) {
        case ParameterType::ENCODER: {
            EncoderParameter* encParam = static_cast<EncoderParameter*>(param);
            if (steps > 0) encParam->Increment(steps);
            else encParam->Decrement(-steps);
            break;
        }
        case ParameterType::POTENTIOMETER: {
            if (param->GetDisplayType() == DisplayType::DISCRETE) {
                // Discrete pots (e.g. Subdivision) step by one whole value per detent.
                param->SetValue(param->GetValue() + steps);
            } else {
                static constexpr float kStepPerTick = 0.01f; // 1% of range per encoder tick
                float normalized = param->GetNormalizedValue() + (steps * kStepPerTick);
                param->SetNormalizedValue(normalized);
            }
            break;
        }
        case ParameterType::TOGGLE: {
            ToggleParameter* toggleParam = static_cast<ToggleParameter*>(param);
            toggleParam->Toggle();
            break;
        }
    }

    if (param->GetDisplayIndex() >= 0) {
        UpdateParameterDisplayEditing(param, param->GetDisplayIndex() + 1);
    }
    currentEffect_->Update();
}

void Perspective::RefreshParameterDisplays() {
    if (!currentEffect_) return;
    for (size_t i = 0; i < currentEffect_->GetParameterCount(); i++) {
        EffectParameter* param = currentEffect_->GetParameter(i);
        if (!param || param->GetDisplayIndex() < 0) continue;
        size_t displayIndex = static_cast<size_t>(param->GetDisplayIndex()) + 1;
        if (static_cast<int>(i) == selectedParamIndex_) {
            if (paramEditMode_) {
                UpdateParameterDisplayEditing(param, displayIndex);
            } else {
                UpdateParameterDisplayHighlighted(param, displayIndex);
            }
        } else {
            UpdateParameterDisplay(param, displayIndex);
        }
    }
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
#if defined(PERSPECTIVE_PLATFORM_AMP)
    ApplyCabSettings();
#endif
    mode_ = PerspectiveMode::EFFECT;
    SetCurrentEffect(currentEffectIndex_);
}

#if defined(PERSPECTIVE_PLATFORM_AMP)
// Push the cab settings into the channel-2 cab sim. Called at startup and on
// leaving settings mode.
void Perspective::ApplyCabSettings() {
    if (!cabSimEffect_ || cabSimEffect_->GetParameterCount() < 5) return;
    if (settingsParameters_.size() <= static_cast<size_t>(kSettingsParamCabLevel)) return;

    // CabSimEffect params: [0]=Low Cut, [1]=Reso, [2]=Presence, [3]=Rolloff, [4]=Level
    cabSimEffect_->GetParameter(3)->SetValue(settingsParameters_[kSettingsParamCabRolloff]->GetValue());
    cabSimEffect_->GetParameter(2)->SetValue(settingsParameters_[kSettingsParamCabPresence]->GetValue());
    cabSimEffect_->GetParameter(4)->SetValue(settingsParameters_[kSettingsParamCabLevel]->GetValue());
    cabSimEffect_->Update();
}
#endif

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
        
        // Refresh display with restored values, re-arming macro knob catch since values changed
        ArmMacroKnobCatch();
        hardware.ClearDisplay();
        hardware.SetParameterDisplay(0, currentEffect_->GetName(), "");
        RefreshParameterDisplays();
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

    ArmMacroKnobCatch();
    ResetParameterSelection();

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
        // First boot or layout change – start with a blank bank plus whatever
        // factory presets we ship.
        presetBank_.Clear();
        SeedFactoryPresets();
        Hardware::PrintLine("Presets: no valid flash data, seeded factory presets.");
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

// Populate the factory presets. Each entry captures an effect's own default
// parameter values, so the tone lives with the effect rather than being
// duplicated as a table of magic numbers here.
//
// Effects are looked up by name, not index: the factory appends new effects
// over time, so a hardcoded index would eventually point at the wrong one.
void Perspective::SeedFactoryPresets() {
    struct FactoryPreset {
        const char* effectName;
        const char* presetName;
        size_t slot;
    };

    static const FactoryPreset kFactoryPresets[] = {
        {"Sandman", "Sandman", 0},
    };

    for (const auto& factory : kFactoryPresets) {
        for (size_t i = 0; i < effects_.size(); ++i) {
            Effect* effect = effects_[i];
            if (!effect || std::strcmp(effect->GetName(), factory.effectName) != 0) {
                continue;
            }

            size_t count = effect->GetParameterCount();
            if (count > PRESET_MAX_PARAMS) count = PRESET_MAX_PARAMS;

            float values[PRESET_MAX_PARAMS];
            for (size_t p = 0; p < count; ++p) {
                EffectParameter* param = effect->GetParameter(p);
                values[p] = param ? param->GetValue() : 0.0f;
            }

            presetBank_.Save(factory.slot, i, factory.presetName, values, count);
            break;
        }
    }
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
