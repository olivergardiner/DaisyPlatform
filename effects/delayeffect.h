#pragma once

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class DelayEffect : public Effect {
public:
    DelayEffect();
    ~DelayEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    static constexpr size_t MAX_DELAY = 48000 * 2; // 2 seconds max delay at 48kHz
    
    DelayLine<float, MAX_DELAY> delayL_;
    DelayLine<float, MAX_DELAY> delayR_;
};

} // namespace perspective
