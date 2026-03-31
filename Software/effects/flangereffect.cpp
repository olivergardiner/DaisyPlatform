#include "flangereffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kPi = 3.14159265358979323846f;

static inline float Clamp01(float x) {
    return std::max(0.0f, std::min(x, 1.0f));
}

static inline float WrapPhase(float phase) {
    while (phase >= 1.0f) {
        phase -= 1.0f;
    }
    while (phase < 0.0f) {
        phase += 1.0f;
    }
    return phase;
}

static inline float LfoValue(float phase, int shape) {
    phase = WrapPhase(phase);
    switch (shape) {
        case 1: {
            float t = phase * 2.0f;
            return (t < 1.0f) ? (-1.0f + 2.0f * t) : (3.0f - 2.0f * t);
        }
        case 2:
            return -1.0f + 2.0f * phase;
        case 3:
            return (phase < 0.5f) ? 1.0f : -1.0f;
        case 0:
        default:
            return std::sinf(2.0f * kPi * phase);
    }
}

static inline float CubicInterpolate(float y0, float y1, float y2, float y3, float t) {
    float a0 = y3 - y2 - y0 + y1;
    float a1 = y0 - y1 - a0;
    float a2 = y2 - y0;
    float a3 = y1;
    return ((a0 * t + a1) * t + a2) * t + a3;
}

static inline float ReadDelayInterpolated(const float* buffer, int size, float readPos) {
    while (readPos < 0.0f) {
        readPos += static_cast<float>(size);
    }
    while (readPos >= static_cast<float>(size)) {
        readPos -= static_cast<float>(size);
    }

    int x1 = static_cast<int>(readPos);
    float frac = readPos - static_cast<float>(x1);

    int x0 = (x1 - 1 + size) % size;
    int x2 = (x1 + 1) % size;
    int x3 = (x1 + 2) % size;

    return CubicInterpolate(buffer[x0], buffer[x1], buffer[x2], buffer[x3], frac);
}

} // namespace

FlangerEffect::FlangerEffect()
    : Effect("Flanger")
    , writeIndex_(0)
    , lfoPhaseL_(0.0f)
    , lfoPhaseR_(0.33f)
    , feedbackStateL_(0.0f)
    , feedbackStateR_(0.0f) {
}

FlangerEffect::~FlangerEffect() {
}

void FlangerEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    for (int i = 0; i < kMaxDelaySamples; ++i) {
        delayBufferL_[i] = 0.0f;
        delayBufferR_[i] = 0.0f;
    }

    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.45f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Depth", 0.0f, 1.0f, 0.7f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Rate", 0.02f, 2.0f, 0.25f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("K4 Feedback", -0.95f, 0.95f, 0.35f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("K5 Manual ms", 0.2f, 4.0f, 1.2f, PotCurve::LIN, KNOB_5_IDX));
    AddParameter(new EncoderParameter("E2 Wave", 0.0f, 3.0f, 0.0f, 1.0f, ENCODER_2_IDX));

    static const char* kWaveNames[] = {"Sine", "Tri", "Saw", "Square"};
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kWaveNames, 4);

    Update();
}

void FlangerEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) {
            out[i] = in[i];
        }
        return;
    }

    float mix = Clamp01(parameters_[kParamMix]->GetValue());
    float depth = Clamp01(parameters_[kParamDepth]->GetValue());
    float rateHz = parameters_[kParamRate]->GetValue();
    float feedback = parameters_[kParamFeedback]->GetValue();
    float manualMs = parameters_[kParamManualMs]->GetValue();
    int waveShape = static_cast<int>(parameters_[kParamWave]->GetValue());

    float maxSweepMs = 6.0f;
    float phaseInc = rateHz / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float lfo = LfoValue(lfoPhaseL_, waveShape);
        float mod = 0.5f * (lfo + 1.0f);
        float delayMs = manualMs + depth * maxSweepMs * mod;
        float delaySamples = delayMs * sampleRate_ * 0.001f;

        float input = in[i] + feedbackStateL_ * feedback;
        delayBufferL_[writeIndex_] = input;

        float readPos = static_cast<float>(writeIndex_) - delaySamples;
        float wet = ReadDelayInterpolated(delayBufferL_, kMaxDelaySamples, readPos);
        feedbackStateL_ = wet;

        out[i] = in[i] * (1.0f - mix) + wet * mix;

        writeIndex_ = (writeIndex_ + 1) % kMaxDelaySamples;
        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
    }
}

void FlangerEffect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    float mix = Clamp01(parameters_[kParamMix]->GetValue());
    float depth = Clamp01(parameters_[kParamDepth]->GetValue());
    float rateHz = parameters_[kParamRate]->GetValue();
    float feedback = parameters_[kParamFeedback]->GetValue();
    float manualMs = parameters_[kParamManualMs]->GetValue();
    int waveShape = static_cast<int>(parameters_[kParamWave]->GetValue());

    float maxSweepMs = 6.0f;
    float phaseInc = rateHz / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float lfoL = LfoValue(lfoPhaseL_, waveShape);
        float lfoR = LfoValue(lfoPhaseR_, waveShape);

        float modL = 0.5f * (lfoL + 1.0f);
        float modR = 0.5f * (lfoR + 1.0f);

        float delaySamplesL = (manualMs + depth * maxSweepMs * modL) * sampleRate_ * 0.001f;
        float delaySamplesR = (manualMs + depth * maxSweepMs * modR) * sampleRate_ * 0.001f;

        float inputL = inL[i] + feedbackStateL_ * feedback;
        float inputR = inR[i] + feedbackStateR_ * feedback;
        delayBufferL_[writeIndex_] = inputL;
        delayBufferR_[writeIndex_] = inputR;

        float wetL = ReadDelayInterpolated(delayBufferL_, kMaxDelaySamples, static_cast<float>(writeIndex_) - delaySamplesL);
        float wetR = ReadDelayInterpolated(delayBufferR_, kMaxDelaySamples, static_cast<float>(writeIndex_) - delaySamplesR);

        feedbackStateL_ = wetL;
        feedbackStateR_ = wetR;

        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;

        writeIndex_ = (writeIndex_ + 1) % kMaxDelaySamples;
        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
        lfoPhaseR_ = WrapPhase(lfoPhaseR_ + phaseInc * 1.013f);
    }
}

void FlangerEffect::Update() {
    // Parameters are read per-sample in Process for smooth modulation behavior.
}
