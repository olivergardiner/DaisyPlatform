#ifndef PERSPECTIVE_CHORUSEFFECT_H
#define PERSPECTIVE_CHORUSEFFECT_H

#include "effect.h"

namespace perspective {

class ChorusEffect : public Effect {
public:
    ChorusEffect();
    ~ChorusEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    static constexpr int kMaxDelaySamples = 8192;

    float delayBufferL_[kMaxDelaySamples];
    float delayBufferR_[kMaxDelaySamples];
    int writeIndex_;

    float lfoPhaseL_;
    float lfoPhaseR_;

    float feedbackStateL_;
    float feedbackStateR_;
};

} // namespace perspective

#endif // PERSPECTIVE_CHORUSEFFECT_H
