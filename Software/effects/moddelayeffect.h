#ifndef PERSPECTIVE_MODDELAYEFFECT_H
#define PERSPECTIVE_MODDELAYEFFECT_H

#include "tempoeffect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class ModDelayEffect : public TempoEffect {
public:
    ModDelayEffect();
    ~ModDelayEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

protected:
    // Modulation
    Oscillator lfoL_;
    Oscillator lfoR_;
    float currentDelaySamples_ = 0.0f;  // Cache current delay time in samples
};

} // namespace perspective

#endif // PERSPECTIVE_MODDELAYEFFECT_H
