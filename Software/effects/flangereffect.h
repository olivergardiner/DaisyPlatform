#ifndef PERSPECTIVE_FLANGEREFFECT_H
#define PERSPECTIVE_FLANGEREFFECT_H

#include "effect.h"

namespace perspective {

class FlangerEffect : public Effect {
public:
    FlangerEffect();
    ~FlangerEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    static constexpr int kMaxDelaySamples = 2048;

    float delayBufferL_[kMaxDelaySamples];
    float delayBufferR_[kMaxDelaySamples];
    int writeIndex_;

    float lfoPhaseL_;
    float lfoPhaseR_;

    float feedbackStateL_;
    float feedbackStateR_;
};

} // namespace perspective

#endif // PERSPECTIVE_FLANGEREFFECT_H
