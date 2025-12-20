#include "waheffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

WahEffect::WahEffect()
    : Effect("Wah") {
}

WahEffect::~WahEffect() {
}

void WahEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize autowah for stereo
    wahL_.Init(sampleRate);
    wahR_.Init(sampleRate);
    
    // Add parameters: Mix, Wah (expression pedal)
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Wah", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_EXP_IDX));
    
    // Set default wah parameters
    Update();
}

void WahEffect::Process(float* in, float* out, size_t size) {
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
        float wet = wahL_.Process(in[i]);
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void WahEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
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
    
    // Process stereo signal with independent wah
    for (size_t i = 0; i < size; i++) {
        float wetL = wahL_.Process(inL[i]);
        float wetR = wahR_.Process(inR[i]);
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void WahEffect::Update() {
    // Update wah parameter from effect parameters
    if (parameters_.size() >= 2) {
        // Mix parameter (index 0) - handled in Process
        
        // Wah parameter (index 1 / pot 7)
        float wah = parameters_[1]->GetValue();
        wahL_.SetWah(wah);
        wahR_.SetWah(wah);
    }
}
