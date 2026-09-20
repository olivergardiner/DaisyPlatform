#include "motorwaheffect.h"

#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"

#include <cmath>

using namespace perspective;
using namespace daisysp;

namespace {

static const char* lfoWaveShapes[8] = {
    "Sin", "Tri", "Saw", "Ramp", "Square", "PB Tri", "PB Saw", "PB Sq"
};

} // namespace

MotorWahEffect::MotorWahEffect()
    : AutowahV2Effect("Motor Wah") {
}

MotorWahEffect::~MotorWahEffect() {
}

void MotorWahEffect::Init(float sampleRate) {
    InitFilterState(sampleRate, 850.0f);

    lfo_.Init(sampleRate_);
    lfo_.SetAmp(1.0f);
    lfo_.SetWaveform(Oscillator::WAVE_SIN);
    lfo_.SetFreq(0.8f);

    auto* mixParam = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.6f);
    mixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(mixParam);

    AddParameter(new ValueParameter("Resonance", 0.0f, 1.0f, 0.9f));
    AddParameter(new ValueParameter("Frequency", 250.0f, 2000.0f, 850.0f));

    auto* rateParam = new ValueParameter("K3 Rate Hz", 0.05f, 12.0f, 0.8f);
    rateParam->BindPotentiometer(MACRO_KNOB_RATE_IDX, PotCurve::LOG);
    rateParam->SetMacroRole(MacroRole::RATE);
    AddParameter(rateParam);

    auto* depthParam = new ValueParameter("K2 Depth", 0.0f, 3000.0f, 1800.0f);
    depthParam->BindPotentiometer(MACRO_KNOB_DEPTH_IDX, PotCurve::LIN);
    depthParam->SetMacroRole(MacroRole::DEPTH);
    AddParameter(depthParam);

    auto* waveParam = new EnumParameter("E2 Wave", lfoWaveShapes, 8, 0);
    waveParam->BindEncoder(ENCODER_2_IDX);
    AddParameter(waveParam);

    Update();
}

void MotorWahEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            out[i] = in[i];
        }
        return;
    }

    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.6f;
    float resonance = parameters_.size() > 1 ? parameters_[kParamResonance]->GetValue() : 0.9f;
    float baseFreq = parameters_.size() > 2 ? parameters_[kParamFrequency]->GetValue() : 850.0f;
    float depthHz = parameters_.size() > 4 ? parameters_[kParamDepth]->GetValue() : 1800.0f;

    for (size_t i = 0; i < size; ++i) {
        float lfoValue = 0.5f * (lfo_.Process() + 1.0f);
        detectorInput_ = lfoValue;

        float targetCutoff = ComputeTargetCutoffHz(baseFreq, depthHz, lfoValue);
        ProcessWahSample(in[i], mix, resonance, targetCutoff, out[i]);
    }
}

void MotorWahEffect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.6f;
    float resonance = parameters_.size() > 1 ? parameters_[kParamResonance]->GetValue() : 0.9f;
    float baseFreq = parameters_.size() > 2 ? parameters_[kParamFrequency]->GetValue() : 850.0f;
    float depthHz = parameters_.size() > 4 ? parameters_[kParamDepth]->GetValue() : 1800.0f;

    for (size_t i = 0; i < size; ++i) {
        float lfoValue = 0.5f * (lfo_.Process() + 1.0f);
        detectorInput_ = lfoValue;

        float targetCutoff = ComputeTargetCutoffHz(baseFreq, depthHz, lfoValue);
        ProcessWahStereoSample(inL[i], inR[i], mix, resonance, targetCutoff, outL[i], outR[i]);
    }
}

void MotorWahEffect::Update() {
    if (parameters_.size() >= 6) {
        float rateHz = parameters_[kParamRateHz]->GetValue();
        int waveform = static_cast<int>(std::round(parameters_[kParamWave]->GetValue()));

        lfo_.SetFreq(rateHz);
        lfo_.SetWaveform(static_cast<uint8_t>(waveform));
    }

    AutowahV2Effect::Update();
}

float MotorWahEffect::GetTempoPulseBrightness() const {
    return detectorInput_;
}
