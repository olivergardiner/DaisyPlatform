#include "phasereffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kPi = 3.14159265358979323846f;

static inline float Clamp(float x, float lo, float hi) {
    return std::max(lo, std::min(x, hi));
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

static inline float ComputeAllpassCoeff(float freqHz, float sampleRate) {
    float clampedFreq = Clamp(freqHz, 20.0f, 0.45f * sampleRate);
    float g = std::tanf(kPi * clampedFreq / sampleRate);
    float a = (1.0f - g) / (1.0f + g);
    return Clamp(a, -0.98f, 0.98f);
}

} // namespace

PhaserEffect::PhaserEffect()
    : Effect("Phaser")
    , lfoPhaseL_(0.0f)
    , lfoPhaseR_(0.25f)
    , feedbackStateL_(0.0f)
    , feedbackStateR_(0.0f) {
}

PhaserEffect::~PhaserEffect() {
}

void PhaserEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    for (int i = 0; i < kMaxStages; ++i) {
        stagesL_[i].a = 0.0f;
        stagesL_[i].z1 = 0.0f;
        stagesR_[i].a = 0.0f;
        stagesR_[i].z1 = 0.0f;
    }

    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Depth", 0.0f, 1.0f, 0.75f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Rate", 0.02f, 4.0f, 0.3f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("K4 Feedback", -0.95f, 0.95f, 0.35f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("K5 Center Hz", 150.0f, 1200.0f, 450.0f, PotCurve::LOG, KNOB_5_IDX));
    AddParameter(new EncoderParameter("E1 Stages", 2.0f, 8.0f, 4.0f, 1.0f, ENCODER_1_IDX));
    AddParameter(new EncoderParameter("E2 Wave", 0.0f, 3.0f, 0.0f, 1.0f, ENCODER_2_IDX));

    static const char* kWaveNames[] = {"Sine", "Tri", "Saw", "Square"};
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kWaveNames, 4);

    Update();
}

void PhaserEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) {
            out[i] = in[i];
        }
        return;
    }

    float mix = Clamp(parameters_[0]->GetValue(), 0.0f, 1.0f);
    float depth = Clamp(parameters_[1]->GetValue(), 0.0f, 1.0f);
    float rateHz = parameters_[2]->GetValue();
    float feedback = parameters_[3]->GetValue();
    float centerHz = parameters_[4]->GetValue();
    int stages = static_cast<int>(Clamp(parameters_[5]->GetValue(), 2.0f, 8.0f));
    int waveShape = static_cast<int>(parameters_[6]->GetValue());

    if (stages < 2) {
        stages = 2;
    }

    float phaseInc = rateHz / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float lfo = LfoValue(lfoPhaseL_, waveShape);
        float mod = 0.5f * (lfo + 1.0f);

        float minHz = centerHz * (1.0f - 0.85f * depth);
        float maxHz = centerHz * (1.0f + 2.5f * depth);
        minHz = Clamp(minHz, 40.0f, 6000.0f);
        maxHz = Clamp(maxHz, minHz + 1.0f, 8000.0f);

        float sweepHz = minHz + (maxHz - minHz) * mod;

        float x = in[i] + feedbackStateL_ * feedback;
        float y = x;

        for (int s = 0; s < stages; ++s) {
            float spread = static_cast<float>(s + 1) / static_cast<float>(stages);
            float stageHz = sweepHz * (0.65f + spread * 0.9f);
            stagesL_[s].a = ComputeAllpassCoeff(stageHz, sampleRate_);
            y = stagesL_[s].Process(y);
        }

        feedbackStateL_ = y;
        out[i] = in[i] * (1.0f - mix) + y * mix;

        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
    }
}

void PhaserEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    float mix = Clamp(parameters_[0]->GetValue(), 0.0f, 1.0f);
    float depth = Clamp(parameters_[1]->GetValue(), 0.0f, 1.0f);
    float rateHz = parameters_[2]->GetValue();
    float feedback = parameters_[3]->GetValue();
    float centerHz = parameters_[4]->GetValue();
    int stages = static_cast<int>(Clamp(parameters_[5]->GetValue(), 2.0f, 8.0f));
    int waveShape = static_cast<int>(parameters_[6]->GetValue());

    if (stages < 2) {
        stages = 2;
    }

    float phaseInc = rateHz / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float lfoL = LfoValue(lfoPhaseL_, waveShape);
        float lfoR = LfoValue(lfoPhaseR_, waveShape);

        float modL = 0.5f * (lfoL + 1.0f);
        float modR = 0.5f * (lfoR + 1.0f);

        float minHz = centerHz * (1.0f - 0.85f * depth);
        float maxHz = centerHz * (1.0f + 2.5f * depth);
        minHz = Clamp(minHz, 40.0f, 6000.0f);
        maxHz = Clamp(maxHz, minHz + 1.0f, 8000.0f);

        float sweepL = minHz + (maxHz - minHz) * modL;
        float sweepR = minHz + (maxHz - minHz) * modR;

        float xL = inL[i] + feedbackStateL_ * feedback;
        float xR = inR[i] + feedbackStateR_ * feedback;
        float yL = xL;
        float yR = xR;

        for (int s = 0; s < stages; ++s) {
            float spread = static_cast<float>(s + 1) / static_cast<float>(stages);
            float stageHzL = sweepL * (0.65f + spread * 0.9f);
            float stageHzR = sweepR * (0.65f + spread * 0.9f);

            stagesL_[s].a = ComputeAllpassCoeff(stageHzL, sampleRate_);
            stagesR_[s].a = ComputeAllpassCoeff(stageHzR, sampleRate_);

            yL = stagesL_[s].Process(yL);
            yR = stagesR_[s].Process(yR);
        }

        feedbackStateL_ = yL;
        feedbackStateR_ = yR;

        outL[i] = inL[i] * (1.0f - mix) + yL * mix;
        outR[i] = inR[i] * (1.0f - mix) + yR * mix;

        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
        lfoPhaseR_ = WrapPhase(lfoPhaseR_ + phaseInc * 1.01f);
    }
}

void PhaserEffect::Update() {
    // Parameters are read per-sample in Process for smooth modulation behavior.
}
