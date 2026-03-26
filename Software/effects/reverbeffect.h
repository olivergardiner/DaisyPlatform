#ifndef PERSPECTIVE_REVERBEFFECT_H
#define PERSPECTIVE_REVERBEFFECT_H

#include "effect.h"
#include "daisysp.h"
#include "daisysp-lgpl.h"

namespace perspective {

class ReverbEffect : public Effect {
public:
    ReverbEffect();
    ~ReverbEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    void OnSelected() override;
    void OnDeselected() override;

    void SetMix(float mix);
    void SetFeedback(float feedback);
    void SetLpFreq(float lpFreq);

private:
    enum ParamIndex {
        kParamMix = 0,
        kParamFeedback,
        kParamCutoffHz
    };

    daisysp::ReverbSc* reverb_;
    float mix_;
    float feedback_;
    float lpFreq_;
    int instanceId_;
    volatile bool reverbAllocated_;

    bool TryAllocateReverb();
    void ReleaseReverb();
};

} // namespace perspective

#endif // PERSPECTIVE_REVERBEFFECT_H