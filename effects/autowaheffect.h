#ifndef PERSPECTIVE_AUTOWAHEFFECT_H
#define PERSPECTIVE_AUTOWAHEFFECT_H

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class AutowahEffect : public Effect {
public:
    AutowahEffect();
    ~AutowahEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Svf filterL_;
    Svf filterR_;
    float envelope_;
};

} // namespace perspective

#endif // PERSPECTIVE_AUTOWAHEFFECT_H
