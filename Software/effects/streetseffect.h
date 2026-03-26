#ifndef PERSPECTIVE_STREETSEFFECT_H
#define PERSPECTIVE_STREETSEFFECT_H

#include "compoundeffect.h"
#include "paralleldelayeffect.h"
#include "slapbackdelayeffect.h"

namespace perspective {

class StreetsEffect : public CompoundEffect {
public:
    StreetsEffect();
    ~StreetsEffect() override;

    void Init(float sampleRate) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamMix = 0,
        kParamFeedback,
        kParamSubdivision,
        kParamMix2,
        kParamFeedback2,
        kParamSlapMix,
        kParamTime1,
        kParamTime2,
        kParamTempoMode
    };

    ParallelDelayEffect* parallelDelay_;
    SlapbackDelayEffect* slapbackDelay_;

    static constexpr float kReferenceBpm = 125.5f;
    static constexpr float kPrimarySubdivision = 0.75f; // dotted 8th
    static constexpr float kSecondaryLinkedMs = 510.0f;
    static constexpr float kSecondaryRatio = kSecondaryLinkedMs / ((60000.0f / kReferenceBpm) * kPrimarySubdivision);

    // Cached tempo mode states so we can detect changes and update our own display
    bool tempoMode1_ = false;
};

} // namespace perspective

#endif // PERSPECTIVE_STREETSEFFECT_H
