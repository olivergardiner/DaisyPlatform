#include "slapbackdelayeffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/encoderparameter.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

constexpr size_t MAX_DELAY = 48000; // 1 second max delay at 48kHz
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS slapbackDelayL_;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS slapbackDelayR_;

// Epsilon for delay time comparison (0.5 samples is imperceptible)
static constexpr float DELAY_EPSILON = 0.5f;

SlapbackDelayEffect::SlapbackDelayEffect() 
    : Effect("Slapback") {
}

SlapbackDelayEffect::~SlapbackDelayEffect() {
}

void SlapbackDelayEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize delay lines for stereo
    slapbackDelayL_.Init();
    slapbackDelayR_.Init();
    
    // Add parameters: Mix, Feedback, Time
    // Mix: 0-100%
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    // Feedback: 0-70% (slapback typically has minimal feedback)
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 0.70f, 0.25f, PotCurve::LIN, KNOB_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    // Time: 50-300ms (typical slapback range)
    AddParameter(new EncoderParameter("E1 Time", 10.0f, 200.0f, 50.0f, 1.0f, ENCODER_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(1.0f);
    
    Update();
}

void SlapbackDelayEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[kParamFeedback]->GetValue() : 0.25f;
    
    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float delayed = slapbackDelayL_.Read();
        slapbackDelayL_.Write(in[i] + delayed * feedback);
        
        out[i] = wetOnly_ ? (delayed * mix) : (in[i] + delayed * mix);
    }
}

void SlapbackDelayEffect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[kParamFeedback]->GetValue() : 0.25f;
    
    // Process stereo signal with independent delays
    for (size_t i = 0; i < size; i++) {
        float delayedL = slapbackDelayL_.Read();
        float delayedR = slapbackDelayR_.Read();
        
        slapbackDelayL_.Write(inL[i] + delayedL * feedback);
        slapbackDelayR_.Write(inR[i] + delayedR * feedback);
        
        outL[i] = wetOnly_ ? (delayedL * mix) : (inL[i] + delayedL * mix);
        outR[i] = wetOnly_ ? (delayedR * mix) : (inR[i] + delayedR * mix);
    }
}

void SlapbackDelayEffect::Update() {
    // Update delay parameters from effect parameters
    if (parameters_.size() >= 3) {
        // Mix parameter (index 0) - handled in Process
        // Feedback parameter (index 1) - handled in Process
        
        // Time parameter (index 2) - in milliseconds
        float timeMs = parameters_[kParamTimeMs]->GetValue();
        delayTime_ = timeMs / 1000.0f; // Convert ms to seconds
        
        // Update delay line only when parameters change
        float delaySamples = sampleRate_ * delayTime_;
        if (std::abs(delaySamples - currentDelaySamples_) > DELAY_EPSILON) {
            currentDelaySamples_ = delaySamples;
            slapbackDelayL_.SetDelay(delaySamples);
            slapbackDelayR_.SetDelay(delaySamples);
        }
    }
}
