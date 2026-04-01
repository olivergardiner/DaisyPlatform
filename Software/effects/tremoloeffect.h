#ifndef PERSPECTIVE_TREMOLOEFFECT_H
#define PERSPECTIVE_TREMOLOEFFECT_H

#include "effect.h"

namespace perspective {

class TremoloEffect : public Effect {
public:
    TremoloEffect();
    ~TremoloEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamDepth = 0,
        kParamRate,
        kParamWave,
        kParamStereoOffset,
    };

    float lfoPhaseL_;
    float lfoPhaseR_;

    // Cached parameter values
    float depth_;
    float rate_;
    int   wave_;
    float stereoOffset_; // R channel phase offset in cycles (0..0.5)
};

} // namespace perspective

#endif // PERSPECTIVE_TREMOLOEFFECT_H
