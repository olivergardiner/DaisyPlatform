#include "motorwaheffect.h"

#include "../controls.h"

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

    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.6f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Resonance", 0.0f, 1.0f, 0.9f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Frequency", 250.0f, 2000.0f, 850.0f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("K4 Rate Hz", 0.05f, 12.0f, 0.8f, PotCurve::LOG, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("K5 Depth", 0.0f, 3000.0f, 1800.0f, PotCurve::LIN, KNOB_5_IDX));
    AddParameter(new EncoderParameter("E2 Wave", 0.0f, static_cast<float>(Oscillator::WAVE_LAST - 1), 0.0f, 1.0f, ENCODER_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(lfoWaveShapes, 8);

    Update();
}

void MotorWahEffect::Process(float* in, float* out, size_t size) {
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

void MotorWahEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
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
