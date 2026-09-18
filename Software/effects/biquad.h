#ifndef PERSPECTIVE_BIQUAD_H
#define PERSPECTIVE_BIQUAD_H

#include <cmath>

namespace perspective {

// Minimal transposed-direct-form-II biquad with RBJ cookbook designers.
// Header-only so the amp-chain effects can inline it in their sample loops.
class Biquad {
public:
    Biquad() : b0_(1.0f), b1_(0.0f), b2_(0.0f), a1_(0.0f), a2_(0.0f), z1_(0.0f), z2_(0.0f) {}

    inline void Reset() {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    inline float Process(float x) {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

    void SetLowpass(float freq, float q, float sampleRate) {
        float cosw, alpha;
        Common(freq, q, sampleRate, cosw, alpha);
        const float b0 = (1.0f - cosw) * 0.5f;
        Normalize(b0, 1.0f - cosw, b0, 1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    void SetHighpass(float freq, float q, float sampleRate) {
        float cosw, alpha;
        Common(freq, q, sampleRate, cosw, alpha);
        const float b0 = (1.0f + cosw) * 0.5f;
        Normalize(b0, -(1.0f + cosw), b0, 1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    // gain_db < 0 cuts, > 0 boosts, centred on freq with bandwidth set by q.
    void SetPeaking(float freq, float q, float gain_db, float sampleRate) {
        const float A = std::powf(10.0f, gain_db / 40.0f);
        float cosw, alpha;
        Common(freq, q, sampleRate, cosw, alpha);
        Normalize(1.0f + alpha * A, -2.0f * cosw, 1.0f - alpha * A,
                  1.0f + alpha / A, -2.0f * cosw, 1.0f - alpha / A);
    }

    void SetLowShelf(float freq, float slope, float gain_db, float sampleRate) {
        const float A = std::powf(10.0f, gain_db / 40.0f);
        const float w = 2.0f * 3.14159265358979f * freq / sampleRate;
        const float cosw = std::cosf(w);
        const float alpha = std::sinf(w) * 0.5f * std::sqrtf((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrtf(A) * alpha;
        Normalize(A * ((A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha),
                  2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw),
                  A * ((A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha),
                  (A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha,
                  -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw),
                  (A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha);
    }

    void SetHighShelf(float freq, float slope, float gain_db, float sampleRate) {
        const float A = std::powf(10.0f, gain_db / 40.0f);
        const float w = 2.0f * 3.14159265358979f * freq / sampleRate;
        const float cosw = std::cosf(w);
        const float alpha = std::sinf(w) * 0.5f * std::sqrtf((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrtf(A) * alpha;
        Normalize(A * ((A + 1.0f) + (A - 1.0f) * cosw + twoSqrtAAlpha),
                  -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw),
                  A * ((A + 1.0f) + (A - 1.0f) * cosw - twoSqrtAAlpha),
                  (A + 1.0f) - (A - 1.0f) * cosw + twoSqrtAAlpha,
                  2.0f * ((A - 1.0f) - (A + 1.0f) * cosw),
                  (A + 1.0f) - (A - 1.0f) * cosw - twoSqrtAAlpha);
    }

private:
    static void Common(float freq, float q, float sampleRate, float& cosw, float& alpha) {
        // Keep the design frequency below Nyquist so coefficients stay stable
        // when a caller sweeps a cutoff up against the sample rate.
        const float nyquist = sampleRate * 0.5f;
        if (freq > nyquist * 0.98f) freq = nyquist * 0.98f;
        if (freq < 1.0f) freq = 1.0f;
        if (q < 0.05f) q = 0.05f;

        const float w = 2.0f * 3.14159265358979f * freq / sampleRate;
        cosw = std::cosf(w);
        alpha = std::sinf(w) / (2.0f * q);
    }

    void Normalize(float b0, float b1, float b2, float a0, float a1, float a2) {
        const float inv = 1.0f / a0;
        b0_ = b0 * inv;
        b1_ = b1 * inv;
        b2_ = b2 * inv;
        a1_ = a1 * inv;
        a2_ = a2 * inv;
    }

    float b0_, b1_, b2_, a1_, a2_;
    float z1_, z2_;
};

} // namespace perspective

#endif // PERSPECTIVE_BIQUAD_H
