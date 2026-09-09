#ifndef PERSPECTIVE_COMPRESSOREFFECT_H
#define PERSPECTIVE_COMPRESSOREFFECT_H

#include "effect.h"

namespace perspective {

class CompressorEffect : public Effect {
public:
    CompressorEffect();
    ~CompressorEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamThreshold = 0,
        kParamRatio,
        kParamAttack,
        kParamRelease,
        kParamMakeup,
        kParamMix
    };

    // Cached parameter values
    float threshold_db_;
    float ratio_;
    float attack_coeff_;
    float release_coeff_;
    float makeup_lin_;
    float mix_;

    // Envelope state
    float envelope_;    // mono
    float envelopeL_;   // stereo left
    float envelopeR_;   // stereo right

    static float AttackCoeff(float attack_ms, float sampleRate);
    static float ReleaseCoeff(float release_ms, float sampleRate);

    // Compute gain (linear multiplier) for the given detector level (linear)
    float ComputeGain(float level_lin) const;
};

} // namespace perspective

#endif // PERSPECTIVE_COMPRESSOREFFECT_H
