#ifndef PERSPECTIVE_MYSTERIOUSEFFECT_H
#define PERSPECTIVE_MYSTERIOUSEFFECT_H

#include "compoundeffect.h"

namespace perspective {

class MysteriousEffect : public CompoundEffect {
public:
    MysteriousEffect();
    ~MysteriousEffect() override;

    void Init(float sampleRate) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamWahMix = 0,
        kParamSweep,
        kParamFlange,
        kParamMotion,
        kParamEcho,
        kParamSpace,
        kParamTime,
        kParamDownBoost
    };

    class AutowahV2Effect* autowahV2Effect_;
    class FlangerEffect* flangerEffect_;
    class DelayEffect* delayEffect_;
    class ReverbEffect* reverbEffect_;
};

} // namespace perspective

#endif // PERSPECTIVE_MYSTERIOUSEFFECT_H
