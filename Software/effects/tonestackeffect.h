#ifndef PERSPECTIVE_TONESTACKEFFECT_H
#define PERSPECTIVE_TONESTACKEFFECT_H

#include "effect.h"
#include "biquad.h"

namespace perspective {

// Post-drive three-band tone stack: low shelf, sweepable mid bell, high shelf.
//
// This is the tone control you reach for, as distinct from the fixed mid-scoop
// inside DriveEffect that shapes what the clipping stages actually see. Runs at
// base rate — it generates no harmonics, so there is nothing to alias.
class ToneStackEffect : public Effect {
public:
    ToneStackEffect();
    ~ToneStackEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void Update() override;

private:
    enum ParamIndex {
        kParamBass = 0,
        kParamMid,
        kParamMidFreq,
        kParamTreble,
        kParamLevel
    };

    Biquad bass_;
    Biquad mid_;
    Biquad treble_;

    // Cached so Update() can skip redesigning filters that did not move
    float bass_db_;
    float mid_db_;
    float mid_freq_;
    float treble_db_;
    float level_lin_;
};

} // namespace perspective

#endif // PERSPECTIVE_TONESTACKEFFECT_H
