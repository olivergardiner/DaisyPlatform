#ifndef PERSPECTIVE_PHASEREFFECT_H
#define PERSPECTIVE_PHASEREFFECT_H

#include "effect.h"

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
    static constexpr int kMaxStages = 8;

    struct AllpassStage {
        float a = 0.0f;
        float z1 = 0.0f;

        inline float Process(float x) {
            float y = -a * x + z1;
            z1 = x + a * y;
            return y;
        }
    };

    AllpassStage stagesL_[kMaxStages];
    AllpassStage stagesR_[kMaxStages];

    float lfoPhaseL_;
    float lfoPhaseR_;

    float feedbackStateL_;
    float feedbackStateR_;
};

} // namespace perspective

#endif // PERSPECTIVE_PHASEREFFECT_H
