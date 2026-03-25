#ifndef PERSPECTIVE_FLANGERLEGACYEFFECT_H
#define PERSPECTIVE_FLANGERLEGACYEFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class FlangerLegacyEffect : public Effect {
public:
    FlangerLegacyEffect();
    ~FlangerLegacyEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Flanger flangerL_;
    Flanger flangerR_;
};

} // namespace perspective

#endif // PERSPECTIVE_FLANGERLEGACYEFFECT_H
