#ifndef PERSPECTIVE_PHASEREFFECT_H
#define PERSPECTIVE_PHASEREFFECT_H

#include "../effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class PhaserEffect : public Effect {
public:
    PhaserEffect();
    ~PhaserEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

private:
    Phaser phaserL_;
    Phaser phaserR_;
};

} // namespace perspective

#endif // PERSPECTIVE_PHASEREFFECT_H
