#ifndef PERSPECTIVE_CABSIMEFFECT_H
#define PERSPECTIVE_CABSIMEFFECT_H

#include "effect.h"
#include "biquad.h"

namespace perspective {

// Filter-based speaker cabinet emulation.
//
// Not a convolved impulse response — four biquads approximating the parts of a
// 4x12 response that actually matter after a high-gain cascade: a low cut, the
// cone resonance bump, a presence peak, and a steep top-end rolloff. The
// rolloff is the important one; it is what turns the cascade's upper harmonics
// from fizz into grind.
class CabSimEffect : public Effect {
public:
    CabSimEffect();
    ~CabSimEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamLowCut = 0,
        kParamResonance,
        kParamPresence,
        kParamRolloff,
        kParamLevel
    };

    // Butterworth Q values for the 4th-order rolloff
    static constexpr float kButterQ1 = 0.54119610f;
    static constexpr float kButterQ2 = 1.30656296f;

    static constexpr float kResonanceFreq = 110.0f;
    static constexpr float kPresenceFreq = 2400.0f;

    // Per-channel filter state: [0] is mono / left, [1] is right
    struct Channel {
        Biquad lowCut;
        Biquad resonance;
        Biquad presence;
        Biquad rolloffA;
        Biquad rolloffB;
    };

    Channel channels_[2];

    void ProcessChannel(Channel& channel, const float* in, float* out, size_t size);

    // Cached parameter values
    float lowCut_hz_;
    float resonance_db_;
    float presence_db_;
    float rolloff_hz_;
    float level_lin_;
};

} // namespace perspective

#endif // PERSPECTIVE_CABSIMEFFECT_H
