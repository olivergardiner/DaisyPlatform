#ifndef PERSPECTIVE_TWELVESTRINGEFFECT_H
#define PERSPECTIVE_TWELVESTRINGEFFECT_H

#include "effect.h"
#include "daisysp.h"

namespace perspective {

class TwelveStringEffect : public Effect {
public:
    TwelveStringEffect();
    ~TwelveStringEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    void OnSelected() override;

private:
    enum ParamIndex {
        kParamOctaveMix = 0,   // K1: level of the +12-semitone octave layer
        kParamDetuneDepth,     // K2: chorus sweep depth (cents feel)
        kParamDetuneRate,      // K3: chorus LFO rate
        kParamChorusMix,       // K4: overall wet/dry of the chorus detuning layer
        kParamOutputLevel,     // K5: master output trim
    };

    // Pointer to pitch shifter allocated in SDRAM (see twelvestringeffect.cpp)
    daisysp::PitchShifter* pitchShifter_;

    // Chorus detuning buffers
    static constexpr int kChorusBufferSize = 4096;
    float chorusBufferL_[kChorusBufferSize];
    float chorusBufferR_[kChorusBufferSize];
    int   writeIndex_;

    float lfoPhaseL_;  // 0.0 - 1.0
    float lfoPhaseR_;  // starts at 0.25 (90° offset for stereo spread)

    // Cached parameter values set in Update()
    float octaveMix_;
    float detuneDepth_;
    float detuneRate_;
    float chorusMix_;
    float outputLevel_;

    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kMaxSweepMs = 8.0f;  // ±8 ms max chorus sweep
    static constexpr float kBaseDelayMs = 6.0f; // centre delay for chorus

    static inline float WrapPhase(float p) {
        while (p >= 1.0f) p -= 1.0f;
        while (p <  0.0f) p += 1.0f;
        return p;
    }

    static inline float CubicInterp(float y0, float y1, float y2, float y3, float t) {
        float a0 = y3 - y2 - y0 + y1;
        float a1 = y0 - y1 - a0;
        float a2 = y2 - y0;
        return ((a0 * t + a1) * t + a2) * t + y1;
    }

    float ReadDelayInterp(const float* buf, float readPos) const;
};

} // namespace perspective

#endif // PERSPECTIVE_TWELVESTRINGEFFECT_H
