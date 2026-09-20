#include "waheffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"

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
    auto* mixParam = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.5f, 0);
    mixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(mixParam);

    AddParameter(new ValueParameter("Resonance", 0.0f, 1.0f, 0.85f, 1));

    auto* sweepParam = new ValueParameter("EP Sweep", 0.0f, 1.0f, 0.5f, 4);
    sweepParam->BindPotentiometer(KNOB_EXP_IDX, PotCurve::REVERSE_LOG);
    AddParameter(sweepParam);

    AddParameter(new ValueParameter("Low Freq", 80.0f, 2000.0f, 400.0f, 2));
    AddParameter(new ValueParameter("High Freq", 200.0f, 5000.0f, 2000.0f, 3));

    // Set default filter parameters
    Update();
}

void WahEffect::Process(const float* in, float* out, size_t size) {
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

void WahEffect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
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

bool WahEffect::UsesExpressionPedal() const {
    return true;
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
