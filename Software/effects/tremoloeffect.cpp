#include "tremoloeffect.h"

#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kPi = 3.14159265358979323846f;

static inline float WrapPhase(float phase) {
    while (phase >= 1.0f) phase -= 1.0f;
    while (phase <  0.0f) phase += 1.0f;
    return phase;
}

// Returns a unipolar gain value in [0, 1] for the given shape and phase.
// Sine and triangle are smooth; square is hard-clipped.
static inline float LfoGain(float phase, int shape) {
    phase = WrapPhase(phase);
    switch (shape) {
        case 1: {
            // Triangle: ramps 0→1→0
            float t = phase * 2.0f;
            return (t < 1.0f) ? t : (2.0f - t);
        }
        case 2:
            // Square: alternates 0 and 1 (hard tremolo chop)
            return (phase < 0.5f) ? 1.0f : 0.0f;
        case 0:
        default:
            // Sine: maps [-1,1] → [0,1]
            return 0.5f + 0.5f * std::sinf(2.0f * kPi * phase);
    }
}

} // namespace

TremoloEffect::TremoloEffect()
    : Effect("Tremolo")
    , lfoPhaseL_(0.0f)
    , lfoPhaseR_(0.25f)
    , depth_(0.75f)
    , rate_(5.0f)
    , wave_(0)
    , stereoOffset_(0.25f)
{
}

TremoloEffect::~TremoloEffect() {
}

void TremoloEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // K2: Depth — how much the volume is modulated (0 = no tremolo, 1 = cuts to silence)
    auto* depthParam = new ValueParameter("K2 Depth", 0.0f, 1.0f, 0.75f);
    depthParam->BindPotentiometer(MACRO_KNOB_DEPTH_IDX, PotCurve::LIN);
    depthParam->SetMacroRole(MacroRole::DEPTH);
    AddParameter(depthParam);

    // K3: Rate — LFO frequency in Hz
    auto* rateParam = new ValueParameter("K3 Rate", 0.1f, 10.0f, 5.0f);
    rateParam->BindPotentiometer(MACRO_KNOB_RATE_IDX, PotCurve::LOG);
    rateParam->SetMacroRole(MacroRole::RATE);
    AddParameter(rateParam);

    // E1: Wave shape — Sine / Triangle / Square
    static const char* kWaveNames[] = {"Sine", "Tri", "Square"};
    auto* waveParam = new EnumParameter("E1 Wave", kWaveNames, 3, 0);
    waveParam->BindEncoder(ENCODER_1_IDX);
    AddParameter(waveParam);

    // K5: Stereo offset — phase difference between L and R LFOs (0 = mono, 0.5 = full anti-phase)
    auto* stereoParam = new ValueParameter("Stereo", 0.0f, 0.5f, 0.25f);
    stereoParam->SetDisplayType(DisplayType::SCALED);
    stereoParam->SetScaleFactor(200.0f); // shows 0–100 (%)
    AddParameter(stereoParam);

    Update();
}

void TremoloEffect::Update() {
    depth_       = parameters_[kParamDepth]->GetValue();
    rate_        = parameters_[kParamRate]->GetValue();
    wave_        = static_cast<int>(parameters_[kParamWave]->GetValue() + 0.5f);
    stereoOffset_= parameters_[kParamStereoOffset]->GetValue();
}

void TremoloEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    const float phaseInc = rate_ / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float gain = 1.0f - depth_ + depth_ * LfoGain(lfoPhaseL_, wave_);
        out[i] = in[i] * gain;
        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
    }
}

void TremoloEffect::ProcessStereo(const float* inL, const float* inR,
                                   float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    const float phaseInc = rate_ / sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float gainL = 1.0f - depth_ + depth_ * LfoGain(lfoPhaseL_, wave_);
        float gainR = 1.0f - depth_ + depth_ * LfoGain(lfoPhaseL_ + stereoOffset_, wave_);

        outL[i] = inL[i] * gainL;
        outR[i] = inR[i] * gainR;

        lfoPhaseL_ = WrapPhase(lfoPhaseL_ + phaseInc);
    }
}
