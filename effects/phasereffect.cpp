#include "phasereffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

PhaserEffect::PhaserEffect() 
    : Effect("Phaser") {
}

PhaserEffect::~PhaserEffect() {
}

void PhaserEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize phaser modules for stereo
    phaserL_.Init(sampleRate);
    phaserR_.Init(sampleRate);

    // Add parameters: Mix, Rate, Depth, Feedback, Poles
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Rate", 0.01f, 10.0f, 0.3f, PotCurve::LOG, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("Depth", 0.0f, 1.0f, 0.7f, PotCurve::LIN, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("Feedback", 0.0f, 0.95f, 0.7f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new EncoderParameter("Poles", 1.0f, 8.0f, 4.0f, 1.0f, ENCODER_1_IDX));
    
    // Set default phaser parameters
    Update();
}

void PhaserEffect::Process(float* in, float* out, size_t size) {
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
        float wet = phaserL_.Process(in[i]);
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void PhaserEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
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
    
    // Process stereo signal with independent phaser for each channel
    for (size_t i = 0; i < size; i++) {
        float wetL = phaserL_.Process(inL[i]);
        float wetR = phaserR_.Process(inR[i]);
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void PhaserEffect::Update() {
    // Update phaser parameters from effect parameters
    if (parameters_.size() >= 5) {
        // Mix parameter (index 0) - handled in Process
        
        // Rate parameter (index 1)
        float rate = parameters_[1]->GetValue();
        phaserL_.SetLfoFreq(rate);
        phaserR_.SetLfoFreq(rate);
        
        // Depth parameter (index 2)
        float depth = parameters_[2]->GetValue();
        phaserL_.SetLfoDepth(depth);
        phaserR_.SetLfoDepth(depth);
        
        // Feedback parameter (index 3)
        float feedback = parameters_[3]->GetValue();
        phaserL_.SetFeedback(feedback);
        phaserR_.SetFeedback(feedback);
        
        // Poles parameter (index 4)
        int poles = static_cast<int>(parameters_[4]->GetValue());
        phaserL_.SetPoles(poles);
        phaserR_.SetPoles(poles);
    }
}
