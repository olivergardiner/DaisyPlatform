#include "perspective.h"
#include "effect.h"
#include "effectparameter.h"

using namespace perspective;

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    for (size_t i = 0; i < size; i++){
		out[0][i] = in[0][i];
		out[1][i] = in[1][i];
	}
}

Perspective::Perspective() 
    : currentEffect_(nullptr) {

}

Perspective::~Perspective() {

}

void Perspective::Init() {
    hardware.Init(GetEventHandler());

    // Initialize perspective-specific UI elements
    RegisterEventListeners();

    hardware.StartAudio(AudioCallback);
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
                        potParam->SetNormalizedValueWithCurve(event.value);
                    }
                    // Update effect with new parameter value
                    currentEffect_->Update();
                    break;
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
                        if (event.intValue > 0) {
                            encParam->Increment(event.intValue);
                        } else if (event.intValue < 0) {
                            encParam->Decrement(-event.intValue);
                        }
                    }
                    // Update effect with new parameter value
                    currentEffect_->Update();
                    break;
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
                    }
                    // Update effect with new parameter value
                    currentEffect_->Update();
                    break;
                }
            }
        },
        UIEventType::BUTTON_PRESSED
    );
    
    // Register listener for Encoder_1 changes (main effect selection)
    eventHandler_.RegisterListenerByIndex(
        [](const UIEvent& event) {
            // Handle Encoder_1 changes
            int increment = event.intValue;  // Positive=CW, Negative=CCW
            // Add your encoder handling logic here
            // e.g., change effect selection based on increment
        },
        UIEventType::ENCODER_CHANGED,
        0  // Index 0 = Encoder_1
    );
    
    // Register listener for Switch_2 being released (true bypass toggle)
    eventHandler_.RegisterListenerByIndex(
        [](const UIEvent& event) {
            // Handle Switch_2 release
            // Add your button release handling logic here
        },
        UIEventType::BUTTON_RELEASED,
        1  // Index 1 = Switch_2
    );
    
    // Register listener for Switch_3 being pressed (tap tempo)
    eventHandler_.RegisterListenerByIndex(
        [this](const UIEvent& event) {
            HandleTapTempo();
        },
        UIEventType::BUTTON_PRESSED,
        2  // Index 2 = Switch_3
    );

}

void Perspective::toggleBypass() {
    bypassMode_ = !bypassMode_;    
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
