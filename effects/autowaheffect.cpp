#include "autowaheffect.h"

using namespace perspective;
using namespace daisysp;

AutowahEffect::AutowahEffect()
    : Effect("Autowah")
    , envelope_(0.0f) {
}

AutowahEffect::~AutowahEffect() {
}

void AutowahEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize state variable filters for stereo
    filterL_.Init(sampleRate);
    filterR_.Init(sampleRate);
    
    // Add parameters: Mix, Resonance, Range, Direction, Attack, Release, Frequency
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, 0));
    AddParameter(new PotentiometerParameter("Resonance", 0.5f, 20.0f, 2.0f, PotCurve::LOG, 1));
    AddParameter(new PotentiometerParameter("Frequency", 400.0f, 2000.0f, 1000.0f, PotCurve::LOG, 2));
    AddParameter(new ToggleParameter("Direction", true, 3));  // true = up, false = down
    AddParameter(new PotentiometerParameter("Attack", 0.001f, 0.5f, 0.1f, PotCurve::LOG, 4));
    AddParameter(new PotentiometerParameter("Release", 0.0001f, 0.1f, 0.001f, PotCurve::LOG, 5));
    AddParameter(new PotentiometerParameter("Range", 0.0f, 3000.0f, 1600.0f, PotCurve::LIN, 6));
    
    // Set default filter parameters
    Update();
}

void AutowahEffect::Process(float* in, float* out, size_t size) {
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

void AutowahEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
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
    
    // Get parameters
    float baseFreq = parameters_.size() >= 7 ? parameters_[2]->GetValue() : 1000.0f;
    float freqRange = parameters_.size() >= 7 ? parameters_[6]->GetValue() : 1600.0f;
    float attackCoeff = parameters_.size() >= 7 ? parameters_[4]->GetValue() : 0.1f;
    float releaseCoeff = parameters_.size() >= 7 ? parameters_[5]->GetValue() : 0.001f;
    bool directionUp = true;
    if (parameters_.size() >= 7 && parameters_[3]->GetType() == ParameterType::TOGGLE) {
        ToggleParameter* dirParam = static_cast<ToggleParameter*>(parameters_[3]);
        directionUp = dirParam->GetState();
    }
    
    // Process stereo signal with independent filters
    for (size_t i = 0; i < size; i++) {
        // Compute average envelope of both inputs
        float avgInput = (std::abs(inL[i]) + std::abs(inR[i])) * 0.5f;
        
        // Update envelope follower
        if (avgInput > envelope_) {
            envelope_ += attackCoeff * (avgInput - envelope_);
        } else {
            envelope_ += releaseCoeff * (avgInput - envelope_);
        }
        
        // Modulate filter frequency based on envelope and direction
        float modulation = envelope_ * freqRange;
        float modulatedFreq = directionUp ? (baseFreq + modulation) : (baseFreq - modulation);
        modulatedFreq = std::max(20.0f, std::min(modulatedFreq, 20000.0f)); // Clamp to valid range
        filterL_.SetFreq(modulatedFreq);
        filterR_.SetFreq(modulatedFreq);
        
        filterL_.Process(inL[i]);
        filterR_.Process(inR[i]);
        
        float wetL = filterL_.Band();  // Get bandpass output
        float wetR = filterR_.Band();
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void AutowahEffect::Update() {
    // Update filter parameters from effect parameters
    if (parameters_.size() >= 7) {
        // Mix parameter (index 0) - handled in Process
        // Frequency parameter (index 2) - handled in Process
        // Direction parameter (index 3) - handled in Process
        // Attack parameter (index 4) - handled in Process
        // Release parameter (index 5) - handled in Process
        // Range parameter (index 6) - handled in Process
        
        // Resonance parameter (index 1)
        float q = parameters_[1]->GetNormalizedValue();
        filterL_.SetRes(q);
        filterR_.SetRes(q);
    }
}
