#ifndef PERSPECTIVE_MYSTERIOUSEFFECT_H
#define PERSPECTIVE_MYSTERIOUSEFFECT_H

#include "compoundeffect.h"
#include "compressoreffect.h"

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

    CompressorEffect* compressorEffect_;
    class TwelveStringEffect* twelveStringEffect_;
    Effect* wahEffect_;   // AutowahEffect (V1) or AutowahV2Effect (V2) — see MYSTERIOUS_WAH_V2 in .cpp
    class FlangerEffect* flangerEffect_;
    class DelayEffect* delayEffect_;
    class ReverbEffect* reverbEffect_;
};

} // namespace perspective

#endif // PERSPECTIVE_MYSTERIOUSEFFECT_H
