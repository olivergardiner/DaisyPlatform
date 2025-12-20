#include "bandpasseffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

BandpassEffect::BandpassEffect()
    : Effect("Wah2") {
}

BandpassEffect::~BandpassEffect() {
}

void BandpassEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize state variable filters for stereo
    filterL_.Init(sampleRate);
    filterR_.Init(sampleRate);
    
    // Add parameters: Mix, Resonance, Frequency
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Resonance", 0.5f, 20.0f, 2.0f, PotCurve::LOG, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("Frequency", 400.0f, 2000.0f, 1000.0f, PotCurve::LOG, KNOB_EXP_IDX));
    
    // Set default filter parameters
    Update();
}

void BandpassEffect::Process(float* in, float* out, size_t size) {
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
        filterL_.Process(in[i]);
        float wet = filterL_.Band();  // Get bandpass output
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void BandpassEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
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
    
    // Process stereo signal with independent filters
    for (size_t i = 0; i < size; i++) {
        filterL_.Process(inL[i]);
        filterR_.Process(inR[i]);
        
        float wetL = filterL_.Band();  // Get bandpass output
        float wetR = filterR_.Band();
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void BandpassEffect::Update() {
    // Update filter parameters from effect parameters
    if (parameters_.size() >= 3) {
        // Mix parameter (index 0) - handled in Process
        
        // Frequency parameter (index 1)
        float frequency = parameters_[1]->GetValue();
        filterL_.SetFreq(frequency);
        filterR_.SetFreq(frequency);
        
        // Resonance parameter (index 2)
        // Convert Q factor (0.5-20) to resonance (0-1)
        float q = parameters_[2]->GetNormalizedValue();
        filterL_.SetRes(q);
        filterR_.SetRes(q);
    }
}
