#include "choruseffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

ChorusEffect::ChorusEffect() 
    : Effect("Chorus") {
}

ChorusEffect::~ChorusEffect() {
}

void ChorusEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize chorus modules for stereo
    chorusL_.Init(sampleRate);
    chorusR_.Init(sampleRate);

    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Depth", 0.0f, 1.0f, 0.9f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("Rate", 0.1f, 5.0f, 0.3f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("Delay", 0.1f, 5.0f, 0.75f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("Feedback", -0.95f, 0.95f, 0.0f, PotCurve::LIN, KNOB_5_IDX));
    
    // Set default chorus parameters
    Update();
}

void ChorusEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get mix parameter (index 0)
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    
    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float wet = chorusL_.Process(in[i]);
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void ChorusEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get mix parameter (index 0)
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    
    // Process stereo signal with independent chorus for each channel
    for (size_t i = 0; i < size; i++) {
        float wetL = chorusL_.Process(inL[i]);
        float wetR = chorusR_.Process(inR[i]);
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void ChorusEffect::Update() {
    // Update chorus parameters from effect parameters
    if (parameters_.size() >= 5) {
        // Mix parameter (index 0) - handled in Process
        
        // Depth parameter (index 1)
        float depth = parameters_[1]->GetValue();
        chorusL_.SetLfoDepth(depth);
        chorusR_.SetLfoDepth(depth);
        
        // Rate parameter (index 2)
        float rate = parameters_[2]->GetValue();
        chorusL_.SetLfoFreq(rate);
        chorusR_.SetLfoFreq(rate * 1.1f);  // Slightly different for stereo width
        
        // Delay parameter (index 3)
        float delay = parameters_[3]->GetValue();
        chorusL_.SetDelay(delay);
        chorusR_.SetDelay(delay);
        
        // Feedback parameter (index 4)
        float feedback = parameters_[4]->GetValue();
        chorusL_.SetFeedback(feedback);
        chorusR_.SetFeedback(feedback);
    }
}
