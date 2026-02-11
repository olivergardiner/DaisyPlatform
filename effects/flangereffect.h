#ifndef PERSPECTIVE_FLANGEREFFECT_H
#define PERSPECTIVE_FLANGEREFFECT_H

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

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
    Flanger flangerL_;
    Flanger flangerR_;
};

} // namespace perspective

#endif // PERSPECTIVE_FLANGEREFFECT_H
