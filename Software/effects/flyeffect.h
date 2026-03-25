#ifndef PERSPECTIVE_FLYEFFECT_H
#define PERSPECTIVE_FLYEFFECT_H

#include "compoundeffect.h"

namespace perspective {

// Fly Effect: AutowahV2 followed by a tempo-synced delay set to 3/16ths subdivision
class FlyEffect : public CompoundEffect {
public:
    FlyEffect();
    ~FlyEffect() override;

    void Init(float sampleRate) override;
    void Update() override;
    void SetTempo(float tempoHz) override;

private:
    // Pointers to child effects so we can control them
    class AutowahV2Effect* autowahV2Effect_;
    class DelayEffect* delayEffect_;
    int timeParamIndex_;  // Track time parameter for tempo updates
    int tempoModeParamIndex_;  // Track tempo mode toggle parameter
};

} // namespace perspective

#endif // PERSPECTIVE_FLYEFFECT_H
