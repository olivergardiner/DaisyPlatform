#ifndef PERSPECTIVE_SANDMANEFFECT_H
#define PERSPECTIVE_SANDMANEFFECT_H

#include "compoundeffect.h"

namespace perspective {

// Sandman: cascaded asymmetric drive -> tone stack -> gate.
//
// A high-gain rhythm voice in the Enter Sandman mould. The children are held at
// fixed, tuned values and only a handful of macros are exposed at the top level,
// so the preset lands on the sound rather than on twenty knobs.
//
// No cab stage on either platform: in the pedal, the Daisy sits in the FX loop
// of a real amp into a real cab, so this chain's output goes on to be cabbed
// downstream regardless. In amp mode the cab sim is the channel 2 DI fixture,
// outside this chain entirely. CabSimEffect itself stays — it is still used
// there, and is worth keeping as a lightweight alternative alongside a future
// IR-convolution cab.
class SandmanEffect : public CompoundEffect {
public:
    SandmanEffect();
    ~SandmanEffect() override;

    void Init(float sampleRate) override;
    void Update() override;

    float GetEnvelopeBrightness() const override;

private:
    enum ParamIndex {
        kParamLevel = 0,
        kParamGain,
        kParamGate,
        kParamScoop,
        kParamTreble,
        kParamStages
    };

    class NoiseGateEffect* gate_;
    class DriveEffect* drive_;
    class ToneStackEffect* tone_;
};

} // namespace perspective

#endif // PERSPECTIVE_SANDMANEFFECT_H
