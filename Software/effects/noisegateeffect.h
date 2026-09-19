#ifndef PERSPECTIVE_NOISEGATEEFFECT_H
#define PERSPECTIVE_NOISEGATEEFFECT_H

#include "effect.h"

namespace perspective {

// Noise gate with hysteresis and a hold time.
//
// Placed ahead of the drive cascade it does the job a gate does in front of a
// high-gain amp: kills the hiss between phrases and lets muted chugs stop dead
// instead of ringing on. Hysteresis (a separate, lower close threshold) is what
// stops the gate chattering on a decaying note.
class NoiseGateEffect : public Effect {
public:
    NoiseGateEffect();
    ~NoiseGateEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

    float GetEnvelopeBrightness() const override;

private:
    enum ParamIndex {
        kParamThreshold = 0,
        kParamHysteresis,
        kParamAttack,
        kParamHold,
        kParamRelease
    };

    // Detector level thresholds, linear
    float openLevel_;
    float closeLevel_;

    // Gain ramp coefficients
    float attackCoeff_;
    float releaseCoeff_;

    // Hold time in samples
    uint32_t holdSamples_;

    // State. Single detector and single gain ramp even in stereo: the key is
    // mono, and gating the channels independently would let them open and
    // close at different moments and wander the image.
    float detector_;
    float gain_;
    uint32_t holdCounter_;
    bool open_;

    // Advance the detector and gain ramp by one sample, returning the gain
    float NextGain(float keySample);
};

} // namespace perspective

#endif // PERSPECTIVE_NOISEGATEEFFECT_H
