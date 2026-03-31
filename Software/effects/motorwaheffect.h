#ifndef PERSPECTIVE_MOTORWAHEFFECT_H
#define PERSPECTIVE_MOTORWAHEFFECT_H

#include "autowahv2effect.h"

namespace perspective {

class MotorWahEffect : public AutowahV2Effect {
public:
    MotorWahEffect();
    ~MotorWahEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    float GetTempoPulseBrightness() const override;

private:
    enum ParamIndex {
        kParamMix = 0,
        kParamResonance,
        kParamFrequency,
        kParamRateHz,
        kParamDepth,
        kParamWave
    };

    Oscillator lfo_;
};

} // namespace perspective

#endif // PERSPECTIVE_MOTORWAHEFFECT_H
