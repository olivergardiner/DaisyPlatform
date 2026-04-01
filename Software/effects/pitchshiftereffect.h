#ifndef PERSPECTIVE_PITCHSHIFTEREFFECT_H
#define PERSPECTIVE_PITCHSHIFTEREFFECT_H

#include "effect.h"
#include <stdint.h>

namespace perspective {

/**
 * 4-grain Hann-windowed overlap-add (OLA) pitch shifter.
 *
 * Each grain maintains an ABSOLUTE read position that advances at
 * pitchRatio samples/sample, independently of the write pointer.
 * When a grain phase completes (every kGrainSize output samples),
 * its read pointer hard-resets to kStartDelay samples behind the
 * current write head. Hann windows sum to exactly 2.0 for 4 evenly-
 * spaced grains; output is scaled by 0.5 to normalise.
 *
 * Latency ≈ kStartDelay/sampleRate ≈ 21 ms at 48 kHz.
 * SDRAM: 4096 floats × 2 channels × 4 bytes = 32 KB.
 */
class PitchShifterEffect : public Effect {
public:
    // Public so the .cpp can size SDRAM arrays without a forward declaration.
    static constexpr int kNumGrains  = 4;
    static constexpr int kBufSize    = 4096;   // per channel, must be power of 2
    static constexpr int kBufMask    = kBufSize - 1;
    static constexpr int kGrainSize  = 512;    // samples per grain (~10.7 ms @ 48 kHz)
    static constexpr int kStartDelay = 1024;   // read lag behind write (= 2 × kGrainSize)

    PitchShifterEffect();
    ~PitchShifterEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR,
                       float* outL, float* outR, size_t size) override;
    void Update() override;
    void OnSelected() override;

private:
    enum ParamIndex {
        kParamSemitones = 0,
        kParamMix,
    };

    // Per-channel grain engine state
    struct ChannelState {
        float readPos[kNumGrains];  // absolute position in circular buffer
        float phi[kNumGrains];      // grain phase 0..1 (Hann window argument)
        int   writeIdx;             // unbounded write counter (use & kBufMask to index)
    };

    float* bufL_;
    float* bufR_;

    ChannelState stateL_;
    ChannelState stateR_;

    float pitchRatio_;  // 2^(semitones/12)
    float semitones_;
    float mix_;

    // Process one output sample for one channel
    float ProcessSample(float input, float* buf, ChannelState& s);

    void InitChannelState(ChannelState& s);
    void ClearBuffers();

    // Hann window: sin²(π·φ). Four evenly-spaced grains sum to exactly 2.0.
    static inline float Hann(float phi) {
        float s = sinf(3.14159265f * phi);
        return s * s;
    }

    // Linear interpolation into a circular buffer. pos must be in [0, kBufSize).
    static inline float ReadInterp(const float* buf, float pos) {
        int   i0   = static_cast<int>(pos) & kBufMask;
        int   i1   = (i0 + 1) & kBufMask;
        float frac = pos - static_cast<float>(static_cast<int>(pos));
        return buf[i0] + frac * (buf[i1] - buf[i0]);
    }
};

} // namespace perspective

#endif // PERSPECTIVE_PITCHSHIFTEREFFECT_H
