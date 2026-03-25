#ifndef PERSPECTIVE_CHORUSLEGACYEFFECT_H
#define PERSPECTIVE_CHORUSLEGACYEFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class ChorusLegacyEffect : public Effect {
public:
    ChorusLegacyEffect();
    ~ChorusLegacyEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Chorus chorusL_;
    Chorus chorusR_;
};

} // namespace perspective

#endif // PERSPECTIVE_CHORUSLEGACYEFFECT_H
