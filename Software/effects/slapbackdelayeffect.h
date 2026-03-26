#ifndef PERSPECTIVE_SLAPBACKDELAYEFFECT_H
#define PERSPECTIVE_SLAPBACKDELAYEFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class SlapbackDelayEffect : public Effect {
public:
    SlapbackDelayEffect();
    ~SlapbackDelayEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamMix = 0,
        kParamFeedback,
        kParamTimeMs
    };

    float delayTime_ = 0.125f;  // Default 125ms (typical slapback range)
    float currentDelaySamples_ = 0.0f;  // Cache current delay time in samples
};

} // namespace perspective

#endif // PERSPECTIVE_SLAPBACKDELAYEFFECT_H
