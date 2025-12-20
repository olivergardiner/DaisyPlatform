#include "flangereffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

FlangerEffect::FlangerEffect()
    : Effect("Flanger") {
}

FlangerEffect::~FlangerEffect() {
}

void FlangerEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize flanger for stereo
    flangerL_.Init(sampleRate);
    flangerR_.Init(sampleRate);
    
    // Add parameters: Mix, Depth, Rate, Feedback
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Depth", 0.0f, 1.0f, 0.7f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("Rate", 0.05f, 10.0f, 0.3f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_4_IDX));
    
    // Set default flanger parameters
    Update();
}

void FlangerEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    
    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float wet = flangerL_.Process(in[i]);
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void FlangerEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    
    // Process stereo signal with independent flangers
    for (size_t i = 0; i < size; i++) {
        float wetL = flangerL_.Process(inL[i]);
        float wetR = flangerR_.Process(inR[i]);
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void FlangerEffect::Update() {
    // Update flanger parameters from effect parameters
    if (parameters_.size() >= 4) {
        // Mix parameter (index 0) - handled in Process
        
        // Depth parameter (index 1)
        float depth = parameters_[1]->GetValue();
        flangerL_.SetLfoDepth(depth);
        flangerR_.SetLfoDepth(depth);
        
        // Rate parameter (index 2)
        float rate = parameters_[2]->GetValue();
        flangerL_.SetLfoFreq(rate);
        flangerR_.SetLfoFreq(rate);
        
        // Feedback parameter (index 3)
        float feedback = parameters_[3]->GetValue();
        flangerL_.SetFeedback(feedback);
        flangerR_.SetFeedback(feedback);
    }
}
