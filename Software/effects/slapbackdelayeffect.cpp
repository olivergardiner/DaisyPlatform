#include "slapbackdelayeffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
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
    auto* mixParam = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.50f);
    mixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mixParam->SetDisplayType(DisplayType::SCALED);
    mixParam->SetScaleFactor(100.0f);
    mixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(mixParam);

    // Feedback: 0-70% (slapback typically has minimal feedback)
    auto* feedbackParam = new ValueParameter("K4 Feedback", 0.0f, 0.70f, 0.25f);
    feedbackParam->BindPotentiometer(MACRO_KNOB_FEEDBACK_IDX, PotCurve::LIN);
    feedbackParam->SetDisplayType(DisplayType::SCALED);
    feedbackParam->SetScaleFactor(100.0f);
    feedbackParam->SetMacroRole(MacroRole::FEEDBACK);
    AddParameter(feedbackParam);

    // Time: 50-300ms (typical slapback range)
    auto* timeParam = new ValueParameter("E1 Time", 10.0f, 200.0f, 50.0f);
    timeParam->BindEncoder(ENCODER_1_IDX, 1.0f);
    timeParam->SetDisplayType(DisplayType::SCALED);
    timeParam->SetScaleFactor(1.0f);
    AddParameter(timeParam);

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
