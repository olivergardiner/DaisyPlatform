#ifndef PERSPECTIVE_DRIVEEFFECT_H
#define PERSPECTIVE_DRIVEEFFECT_H

#include "effect.h"
#include "biquad.h"

namespace perspective {

// Cascaded asymmetric-clipping preamp.
//
// Two or three identical soft-clip stages run in series at 2x the host sample
// rate, with a mid-scoop filter ahead of each one. Only the clipping cascade is
// oversampled — the tone stack and cab sim downstream run at base rate, since
// they add no harmonics of their own.
//
// The shaper is a rational tanh approximation with a DC bias added before and
// removed after, which is what produces the even-harmonic asymmetry that gives
// a cascaded amp its character.
class DriveEffect : public Effect {
public:
    DriveEffect();
    ~DriveEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

    float GetEnvelopeBrightness() const override;

private:
    enum ParamIndex {
        kParamGain = 0,
        kParamStages,
        kParamAsymmetry,
        kParamScoop,
        kParamScoopFreq,
        kParamLevel
    };

    static constexpr size_t kMaxStages = 3;
    static constexpr float kOversample = 2.0f;

    // Rational (Pade) approximation of tanh, saturating hard outside |x| > 3.
    static inline float SoftClip(float x) {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // One asymmetric stage: bias in, shape, bias out.
    inline float Stage(float x, size_t index) const {
        return SoftClip(x + bias_[index]) - biasOffset_[index];
    }

    // Anti-imaging/anti-aliasing filter order, in cascaded biquad sections.
    // 4 sections = 8th-order. At 2x oversampling the fold frequency (24 kHz at
    // the standard 48 kHz base rate) sits less than an octave above the
    // cutoff, so a gentler filter gives almost no attenuation right where it
    // matters — see DesignFilters() for the numbers.
    static constexpr size_t kFilterSections = 4;

    // All the state a single audio channel needs. Held per channel so the
    // effect can run in stereo without L and R sharing filter memory.
    struct Channel {
        Biquad scoop[kMaxStages];              // mid-scoop ahead of each stage
        Biquad upFilter[kFilterSections];      // anti-imaging, ahead of the cascade
        Biquad downFilter[kFilterSections];    // anti-aliasing, after it — both at 2x
        float dcPrevIn = 0.0f;                 // DC blocker on the cascade output
        float dcPrevOut = 0.0f;
    };

    void DesignFilters();
    void DesignScoop();
    void ProcessChannel(Channel& channel, const float* in, float* out, size_t size,
                        bool trackEnvelope);

    // Cached parameter values
    float stageGain_[kMaxStages];
    float bias_[kMaxStages];
    float biasOffset_[kMaxStages];  // SoftClip(bias) — removed to keep the stage DC-free
    size_t stageCount_;
    float level_lin_;
    float scoop_db_;
    float scoop_freq_;
    size_t designedStages_;  // stage count the scoop filters were designed for

    // Per-channel state: [0] is mono / left, [1] is right
    Channel channels_[2];

    // Envelope for the front-panel LED, tracked from the left channel only
    float envelope_;
};

} // namespace perspective

#endif // PERSPECTIVE_DRIVEEFFECT_H
