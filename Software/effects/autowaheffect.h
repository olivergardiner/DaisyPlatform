#ifndef PERSPECTIVE_AUTOWAHEFFECT_H
#define PERSPECTIVE_AUTOWAHEFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class AutowahEffect : public Effect {
public:
    AutowahEffect();
    ~AutowahEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    float GetEnvelopeBrightness() const override;

private:
    enum ParamIndex {
        kParamMix = 0,
        kParamResonance,
        kParamFrequency,
        kParamAttackMs,
        kParamReleaseMs,
        kParamSensitivity
    };

    Svf filterL_;
    Svf filterR_;
    float envelope_;
    float detectorInput_ = 0.0f;
};

} // namespace perspective

#endif // PERSPECTIVE_AUTOWAHEFFECT_H
