#include "delayeffect.h"

using namespace perspective;
using namespace daisysp;

DelayEffect::DelayEffect() 
    : Effect("Delay") {
}

DelayEffect::~DelayEffect() {
}

void DelayEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize delay lines for stereo
    delayL_.Init();
    delayR_.Init();
    
    // Add parameters: Mix, Time, Feedback
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, 0));
    AddParameter(new EncoderParameter("Time", 0.001f, 2.0f, 0.5f, 0.01f, 1));
    AddParameter(new PotentiometerParameter("Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, 2));
    
    // Set default delay parameters
    Update();
}

void DelayEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 2 ? parameters_[2]->GetValue() : 0.5f;
    
    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float delayed = delayL_.Read();
        delayL_.Write(in[i] + delayed * feedback);
        
        out[i] = in[i] * (1.0f - mix) + delayed * mix;
    }
}

void DelayEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 2 ? parameters_[2]->GetValue() : 0.5f;
    
    // Process stereo signal with independent delays
    for (size_t i = 0; i < size; i++) {
        float delayedL = delayL_.Read();
        float delayedR = delayR_.Read();
        
        delayL_.Write(inL[i] + delayedL * feedback);
        delayR_.Write(inR[i] + delayedR * feedback);
        
        outL[i] = inL[i] * (1.0f - mix) + delayedL * mix;
        outR[i] = inR[i] * (1.0f - mix) + delayedR * mix;
    }
}

void DelayEffect::Update() {
    // Update delay parameters from effect parameters
    if (parameters_.size() >= 3) {
        // Mix parameter (index 0) - handled in Process
        
        // Time parameter (index 1) - in seconds
        float time = parameters_[1]->GetValue();
        float delaySamples = sampleRate_ * time;
        delayL_.SetDelay(delaySamples);
        delayR_.SetDelay(delaySamples);
        
        // Feedback parameter (index 2) - handled in Process
    }
}
