#pragma once

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class DelayEffect : public Effect {
public:
    DelayEffect();
    ~DelayEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    static constexpr size_t MAX_DELAY = 48000 * 2; // 2 seconds max delay at 48kHz
    
    DelayLine<float, MAX_DELAY> delayL_;
    DelayLine<float, MAX_DELAY> delayR_;
    
    // Modulation
    Oscillator lfoL_;
    Oscillator lfoR_;
    float baseDelayTime_;
    float effectiveDelayTime_;  // Cached effective delay time (updated in Update())
    
    // Tempo mode
    bool tempoMode_;  // false = Time mode (seconds), true = Tempo mode (BPM-based)
    float CalculateDelayTimeFromTempo();
    float GetSubdivisionMultiplier() const;
    
    // Note subdivisions (in sixteenths)
    static constexpr float SUBDIVISION_1_16TH = 0.25f;   // 1 sixteenth
    static constexpr float SUBDIVISION_2_16TH = 0.5f;    // 2 sixteenths (eighth)
    static constexpr float SUBDIVISION_3_16TH = 0.75f;   // 3 sixteenths (dotted eighth)
    static constexpr float SUBDIVISION_4_16TH = 1.0f;    // 4 sixteenths (quarter) - Default
    static constexpr float SUBDIVISION_5_16TH = 1.25f;   // 5 sixteenths
    static constexpr float SUBDIVISION_6_16TH = 1.5f;    // 6 sixteenths (dotted quarter)
    static constexpr float SUBDIVISION_8_16TH = 2.0f;    // 8 sixteenths (half)
};

} // namespace perspective
