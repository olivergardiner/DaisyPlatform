#pragma once

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class BandpassEffect : public Effect {
public:
    BandpassEffect();
    ~BandpassEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Svf filterL_;
    Svf filterR_;
};

} // namespace perspective
