#ifndef PERSPECTIVE_AUTOWAHV2EFFECT_H
#define PERSPECTIVE_AUTOWAHV2EFFECT_H

#include "effect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

class AutowahV2Effect : public Effect {
public:
    AutowahV2Effect();
    ~AutowahV2Effect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    float GetTempoPulseBrightness() const override;

protected:
    explicit AutowahV2Effect(const char* name);

    void InitFilterState(float sampleRate, float initialCutoffHz);
    void ProcessWahSample(float input, float mix, float resonance, float targetCutoffHz, float& output, bool useCompensatedResonance = true);
    void ProcessWahStereoSample(float inputL, float inputR, float mix, float resonance, float targetCutoffHz, float& outputL, float& outputR, bool useCompensatedResonance = true);

    static float ClampValue(float x, float lo, float hi);
    static float OnePoleAlpha(float cutoffHz, float sampleRate);
    static float TimeConstantAlpha(float timeSec, float sampleRate);

    float ProcessDetectorHighpass(float input, float& lowpassState) const;
    float ComputeTargetCutoffHz(float baseFreq, float sensitivityHz, float envelope, bool useLogCurve = true) const;
    float ComputeCompensatedResonance(float resonance, float cutoffHz) const;

    Svf filterL_;
    Svf filterR_;

    float envelope_;
    float detectorInput_;
    float detectorLpStateL_;
    float detectorLpStateR_;
    float cutoffSmoothedHz_;

    float detectorHpAlpha_;
    float cutoffSlewAlpha_;
};

} // namespace perspective

#endif // PERSPECTIVE_AUTOWAHV2EFFECT_H
