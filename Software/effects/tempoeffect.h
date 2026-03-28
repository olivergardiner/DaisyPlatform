#ifndef PERSPECTIVE_TEMPOEFFECT_H
#define PERSPECTIVE_TEMPOEFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

// Abstract base class for effects that use tempo and subdivision
class TempoEffect : public Effect {
public:
    TempoEffect(const char* name);
    virtual ~TempoEffect();
    float GetTempoPulseBrightness() const override;
    bool HasTempoMode() const override { return true; }
    void SetMetronomeLevel(float level) override { metronomeLevel_ = level; }

    // Canonical subdivision glyph strings (shared by all delay-based effects)
    static const char* kSubdivisionGlyphs[8];

protected:
    // Tempo-related state
    float baseDelayTime_ = 0.5f;
    float effectiveDelayTime_;  // Cached effective delay time (updated in Update())
    bool tempoMode_ = false;    // false = Time mode (seconds), true = Tempo mode (BPM-based)
    // Note: metronomeEnabled_ is now inherited from Effect base class (global state from Perspective)

    // Metronome sound generators
    AnalogBassDrum metronomeDrum_;
    AnalogSnareDrum metronomeSnare_;
    HiHat<> metronomeHiHat_;
    float clickEnv_ = 0.0f;
    float clickDecay_ = 0.0f;

    float samplesUntilNextBeat_ = 0.0f;
    float metronomeLevel_ = 0.7f;  // Mix level for metronome
    float tempoPulseBrightness_ = 0.0f;

    // Get metronome sample for current audio frame (call once per sample in derived classes)
    // Returns the metronome audio sample to mix with effect output
    float ProcessMetronome();

    // Calculate delay time based on tempo and subdivision
    float CalculateDelayTimeFromTempo();
    
    // Get subdivision multiplier based on current subdivision parameter
    float GetSubdivisionMultiplier() const;
    
    // Note subdivisions (in sixteenths)
    static constexpr float SUBDIVISION_1_16TH = 0.25f;   // 1 sixteenth
    static constexpr float SUBDIVISION_2_16TH = 0.5f;    // 2 sixteenths (eighth)
    static constexpr float SUBDIVISION_3_16TH = 0.75f;   // 3 sixteenths (dotted eighth)
    static constexpr float SUBDIVISION_4_16TH = 1.0f;    // 4 sixteenths (quarter) - Default
    static constexpr float SUBDIVISION_5_16TH = 1.25f;   // 5 sixteenths
    static constexpr float SUBDIVISION_6_16TH = 1.5f;    // 6 sixteenths (dotted quarter)
    static constexpr float SUBDIVISION_7_16TH = 1.75f;   // 7 sixteenths (double-dotted quarter)
    static constexpr float SUBDIVISION_8_16TH = 2.0f;    // 8 sixteenths (half)
    
    // Subdivision parameter index (must be set by derived classes)
    int subdivisionParamIndex_ = -1;
    int timeParamIndex_ = -1;

    static constexpr float TEMPO_PULSE_DECAY_TIME = 0.12f;
};

} // namespace perspective

#endif // PERSPECTIVE_TEMPOEFFECT_H
