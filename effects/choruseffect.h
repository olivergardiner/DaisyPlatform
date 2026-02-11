#ifndef PERSPECTIVE_CHORUSEFFECT_H
#define PERSPECTIVE_CHORUSEFFECT_H

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

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
    Chorus chorusL_;
    Chorus chorusR_;
};

} // namespace perspective

#endif // PERSPECTIVE_CHORUSEFFECT_H
