#ifndef PERSPECTIVE_SANDMANEFFECT_H
#define PERSPECTIVE_SANDMANEFFECT_H

#include "compoundeffect.h"

namespace perspective {

// Sandman: gate -> cascaded asymmetric drive -> tone stack -> cab sim.
//
// A high-gain rhythm voice in the Enter Sandman mould. The children are held at
// fixed, tuned values and only a handful of macros are exposed at the top level,
// so the preset lands on the sound rather than on twenty knobs.
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
        kParamPresence,
        kParamStages
    };

    class NoiseGateEffect* gate_;
    class DriveEffect* drive_;
    class ToneStackEffect* tone_;
    class CabSimEffect* cab_;
};

} // namespace perspective

#endif // PERSPECTIVE_SANDMANEFFECT_H
