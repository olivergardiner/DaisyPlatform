#ifndef PERSPECTIVE_WAHEFFECT_H
#define PERSPECTIVE_WAHEFFECT_H

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class WahEffect : public Effect {
public:
    WahEffect();
    ~WahEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Autowah wahL_;
    Autowah wahR_;
};

} // namespace perspective

#endif // PERSPECTIVE_WAHEFFECT_H
