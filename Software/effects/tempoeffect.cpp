#include "tempoeffect.h"
#include "../parameters/timeparameter.h"
#include "../parameters/potentiometerparameter.h"

using namespace perspective;

const char* TempoEffect::kSubdivisionGlyphs[8] = {
    "_", "^", "^ `", "]", "] _", "] `", "] ` `", "\\"
};

TempoEffect::TempoEffect(const char* name)
    : Effect(name) {
    // Initialize metronome bass drum (will be properly initialized in derived class Init())
}

TempoEffect::~TempoEffect() {
}

float TempoEffect::GetTempoPulseBrightness() const {
    return tempoPulseBrightness_;
}

float TempoEffect::CalculateDelayTimeFromTempo() {
    if (timeParamIndex_ < 0 || static_cast<size_t>(timeParamIndex_) >= parameters_.size()) {
        return 0.5f; // Default fallback
    }
    
    // Get BPM from TimeParameter
    TimeParameter* timeParam = static_cast<TimeParameter*>(parameters_[timeParamIndex_]);
    float bpm = timeParam->GetValueAsBPM();
    
    // Use tempo_ from Effect base class if available (from tap tempo)
    if (tempo_ > 0.0f) {
        bpm = tempo_ * 60.0f; // Convert Hz to BPM
    }
    
    // Calculate delay time for quarter note at this tempo
    // 60 seconds / BPM = seconds per beat (quarter note)
    float secondsPerBeat = 60.0f / bpm;
    
    // Apply subdivision multiplier
    float subdivisionMultiplier = GetSubdivisionMultiplier();
    
    return secondsPerBeat * subdivisionMultiplier;
}

float TempoEffect::GetSubdivisionMultiplier() const {
    if (subdivisionParamIndex_ < 0 || 
        static_cast<size_t>(subdivisionParamIndex_) >= parameters_.size() ||
        parameters_[subdivisionParamIndex_]->GetType() != ParameterType::POTENTIOMETER) {
        return SUBDIVISION_4_16TH; // Default to quarter note (4 sixteenths)
    }
    
    PotentiometerParameter* subdivParam = static_cast<PotentiometerParameter*>(parameters_[subdivisionParamIndex_]);
    int subdivIndex = subdivParam->GetValueAsInt(7); // 0-7 for 8 subdivisions
    
    switch (subdivIndex) {
        case 0: return SUBDIVISION_1_16TH;  // 1 sixteenth
        case 1: return SUBDIVISION_2_16TH;  // 2 sixteenths (eighth)
        case 2: return SUBDIVISION_3_16TH;  // 3 sixteenths (dotted eighth)
        case 3: return SUBDIVISION_4_16TH;  // 4 sixteenths (quarter) - Default
        case 4: return SUBDIVISION_5_16TH;  // 5 sixteenths
        case 5: return SUBDIVISION_6_16TH;  // 6 sixteenths (dotted quarter)
        case 6: return SUBDIVISION_7_16TH;  // 7 sixteenths (double-dotted quarter)
        case 7: return SUBDIVISION_8_16TH;  // 8 sixteenths (half)
        default: return SUBDIVISION_4_16TH;
    }
}

float TempoEffect::ProcessMetronome() {
    if (!tempoMode_) {
        tempoPulseBrightness_ = 0.0f;
        return 0.0f;
    }
    
    // Get BPM from TimeParameter or tap tempo
    float bpm = 120.0f; // Default
    if (timeParamIndex_ >= 0 && static_cast<size_t>(timeParamIndex_) < parameters_.size()) {
        TimeParameter* timeParam = static_cast<TimeParameter*>(parameters_[timeParamIndex_]);
        bpm = timeParam->GetValueAsBPM();
    }
    
    // Use tap tempo if available
    if (tempo_ > 0.0f) {
        bpm = tempo_ * 60.0f; // Convert Hz to BPM
    }
    
    // Calculate samples per quarter note beat
    float secondsPerBeat = 60.0f / bpm;
    float samplesPerBeat = sampleRate_ * secondsPerBeat;
    
    // Countdown to next beat
    samplesUntilNextBeat_ -= 1.0f;
    
    // Trigger drum on beat
    if (samplesUntilNextBeat_ <= 0.0f) {
        if (metronomeEnabled_) {
            metronomeDrum_.Trig();
        }
        samplesUntilNextBeat_ = samplesPerBeat;
        tempoPulseBrightness_ = 1.0f;
    }

    float pulseDecayPerSample = 1.0f / (sampleRate_ * TEMPO_PULSE_DECAY_TIME);
    tempoPulseBrightness_ = fmaxf(0.0f, tempoPulseBrightness_ - pulseDecayPerSample);
    
    if (!metronomeEnabled_) {
        return 0.0f;
    }

    // Get metronome sample and apply level
    return metronomeDrum_.Process() * metronomeLevel_;
}

