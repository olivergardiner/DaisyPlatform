#ifndef PERSPECTIVE_PARALLELDELAYEFFECT_H
#define PERSPECTIVE_PARALLELDELAYEFFECT_H

#include "compoundeffect.h"
#include "delayeffect.h"

namespace perspective {

class ParallelDelayEffect : public CompoundEffect {
public:
    ParallelDelayEffect();
    ~ParallelDelayEffect() override;

    void Init(float sampleRate) override;
    void Update() override;

private:
    // Child delay effects
    DelayEffect* delay1_;
    DelayEffect* delay2_;
    
    bool tempoMode1_ = false;  // Delay 1: false = Time mode (ms), true = Tempo mode (BPM-based)
    bool tempoMode2_ = false;  // Delay 2: false = Time mode (ms), true = Tempo mode (BPM-based)
    
    // Cached parameter values to detect changes
    float lastMix1_ = -1.0f;
    float lastFeedback1_ = -1.0f;
    float lastSubdivision1_ = -1.0f;
    float lastDelayTime1_ = -1.0f;
    float lastMix2_ = -1.0f;
    float lastFeedback2_ = -1.0f;
    float lastSubdivision2_ = -1.0f;
    float lastDelayTime2_ = -1.0f;
};

} // namespace perspective

#endif // PERSPECTIVE_PARALLELDELAYEFFECT_H
