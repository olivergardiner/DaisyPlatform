#include "waheffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

WahEffect::WahEffect()
    : Effect("Wah") {
}

WahEffect::~WahEffect() {
}

void WahEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // Initialize state variable filters for stereo
    filterL_.Init(sampleRate);
    filterR_.Init(sampleRate);

    // Add parameters: Mix, Resonance, EP sweep, Low Freq, High Freq
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX, 0));
    AddParameter(new PotentiometerParameter("K2 Resonance", 0.0f, 1.0f, 0.85f, PotCurve::LIN, KNOB_2_IDX, 1));
    AddParameter(new PotentiometerParameter("EP Sweep", 0.0f, 1.0f, 0.5f, PotCurve::REVERSE_LOG, KNOB_EXP_IDX, 4));
    AddParameter(new PotentiometerParameter("K4 Low Freq", 80.0f, 2000.0f, 400.0f, PotCurve::LOG, KNOB_4_IDX, 2));
    AddParameter(new PotentiometerParameter("K5 High Freq", 200.0f, 5000.0f, 2000.0f, PotCurve::LOG, KNOB_5_IDX, 3));

    // Set default filter parameters
    Update();
}

void WahEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }

    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;

    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        filterL_.Process(in[i]);
        float wet = filterL_.Band();
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void WahEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;

    // Process stereo signal with independent filters
    for (size_t i = 0; i < size; i++) {
        filterL_.Process(inL[i]);
        filterR_.Process(inR[i]);

        float wetL = filterL_.Band();
        float wetR = filterR_.Band();

        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void WahEffect::Update() {
    // Update filter parameters from effect parameters
    if (parameters_.size() >= 5) {
        // Mix parameter (index 0) - handled in Process

        float lowFreq = parameters_[kParamLowFreq]->GetValue();
        float highFreq = parameters_[kParamHighFreq]->GetValue();
        if (highFreq < lowFreq + 10.0f) {
            highFreq = lowFreq + 10.0f;
        }

        // Keep the expression sweep position normalized, but display/value in true frequency.
        EffectParameter* sweepParam = parameters_[kParamSweep];
        float sweepNormalized = sweepParam->GetNormalizedValue();
        sweepParam->SetRange(lowFreq, highFreq);
        sweepParam->SetNormalizedValue(sweepNormalized);

        float frequency = sweepParam->GetValue();
        filterL_.SetFreq(frequency);
        filterR_.SetFreq(frequency);

        // Ensure EP Sweep display reflects the current mapped frequency.
        RequestParameterDisplayUpdate(2);

        // Resonance parameter (index 1) maps directly to the SVF resonance input
        float resonance = parameters_[kParamResonance]->GetValue();
        filterL_.SetRes(resonance);
        filterR_.SetRes(resonance);
    }
}
