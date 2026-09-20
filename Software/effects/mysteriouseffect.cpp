#include "mysteriouseffect.h"
#include "compressoreffect.h"

// ---- Compile-time wah version switch ----
// Set to 1 to use AutowahV2Effect, 0 to use AutowahEffect (V1)
#define MYSTERIOUS_WAH_V2 1
// -----------------------------------------

#include "autowaheffect.h"
#include "autowahv2effect.h"
#include "delayeffect.h"
#include "flangereffect.h"
#include "reverbeffect.h"
#include "twelvestringeffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"
#include "../parameters/timeparameter.h"

using namespace perspective;

namespace {

static const char* kDownBoostLabels[4] = {
    "Off", "Subtle", "Strong", "Max"
};

} // namespace

MysteriousEffect::MysteriousEffect()
    : CompoundEffect("Mysterious", RoutingMode::SERIES)
    , compressorEffect_(new CompressorEffect())
    , twelveStringEffect_(new TwelveStringEffect())
#if MYSTERIOUS_WAH_V2
    , wahEffect_(new AutowahV2Effect())
#else
    , wahEffect_(new AutowahEffect())
#endif
    , flangerEffect_(new FlangerEffect())
    , delayEffect_(new DelayEffect())
    , reverbEffect_(new ReverbEffect()) {
    AddEffect(compressorEffect_);
    AddEffect(twelveStringEffect_);
    AddEffect(wahEffect_);
    AddEffect(flangerEffect_);
    AddEffect(delayEffect_);
    AddEffect(reverbEffect_);
}

MysteriousEffect::~MysteriousEffect() {
}

void MysteriousEffect::Init(float sampleRate) {
    CompoundEffect::Init(sampleRate);

    auto* wahMixParam = new ValueParameter("K1 Wah Mix", 0.0f, 1.0f, 0.62f, 0);
    wahMixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    wahMixParam->SetDisplayType(DisplayType::SCALED);
    wahMixParam->SetScaleFactor(100.0f);
    wahMixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(wahMixParam);

    AddParameter(new ValueParameter("Sweep", 300.0f, 2600.0f, 1100.0f, 1));

    auto* flangeParam = new ValueParameter("Flange", 0.0f, 1.0f, 0.20f, 2);
    flangeParam->SetDisplayType(DisplayType::SCALED);
    flangeParam->SetScaleFactor(100.0f);
    AddParameter(flangeParam);

    AddParameter(new ValueParameter("Motion", 0.05f, 0.8f, 0.14f, 3));

    auto* echoParam = new ValueParameter("Echo", 0.0f, 0.65f, 0.22f, 4);
    echoParam->SetDisplayType(DisplayType::SCALED);
    echoParam->SetScaleFactor(100.0f);
    AddParameter(echoParam);

    auto* spaceParam = new ValueParameter("K6 Space", 0.0f, 0.75f, 0.34f, 5);
    spaceParam->BindPotentiometer(KNOB_6_IDX, PotCurve::LIN);
    spaceParam->SetDisplayType(DisplayType::SCALED);
    spaceParam->SetScaleFactor(100.0f);
    AddParameter(spaceParam);

    auto* timeParam = new TimeParameter("E1 Time", 80.0f, 450.0f, 230.0f, "E1 Tempo", 6);
    timeParam->BindEncoder(ENCODER_1_IDX, 1.0f);
    AddParameter(timeParam);

    auto* downBoostParam = new EnumParameter("E2 Down+", kDownBoostLabels, 4, 2, 7);
    downBoostParam->BindEncoder(ENCODER_2_IDX);
    AddParameter(downBoostParam);

    Update();
}

void MysteriousEffect::Update() {
    if (parameters_.size() < 8) {
        return;
    }

    float wahMix = parameters_[kParamWahMix]->GetValue();
    float sweepHz = parameters_[kParamSweep]->GetValue();
    float flangeAmount = parameters_[kParamFlange]->GetValue();
    float motionRate = parameters_[kParamMotion]->GetValue();
    float delayMix = parameters_[kParamEcho]->GetValue();
    float space = parameters_[kParamSpace]->GetValue();
    TimeParameter* delayTime = static_cast<TimeParameter*>(parameters_[kParamTime]);
    float downBoost = parameters_[kParamDownBoost]->GetValue();

    // Light front-end compression: 2:1, -18 dB threshold, fast attack, gentle release
    if (compressorEffect_ && compressorEffect_->GetParameterCount() >= 6) {
        compressorEffect_->GetParameter(0)->SetValue(-18.0f);  // Threshold
        compressorEffect_->GetParameter(1)->SetValue(2.0f);    // Ratio 2:1
        compressorEffect_->GetParameter(2)->SetValue(5.0f);    // Attack 5 ms
        compressorEffect_->GetParameter(3)->SetValue(80.0f);   // Release 80 ms
        compressorEffect_->GetParameter(4)->SetValue(2.0f);    // Makeup 2 dB
        compressorEffect_->GetParameter(5)->SetValue(1.0f);    // Mix 100%
        compressorEffect_->Update();
    }

    if (twelveStringEffect_ && twelveStringEffect_->GetParameterCount() >= 5) {
        twelveStringEffect_->GetParameter(0)->SetValue(1.0f);   // Octave 100%
        twelveStringEffect_->GetParameter(1)->SetValue(0.4f);   // Detune 40%
        twelveStringEffect_->GetParameter(2)->SetValue(0.4f);   // Rate 0.4 Hz
        twelveStringEffect_->GetParameter(3)->SetValue(0.4f);   // Chorus 40%
        twelveStringEffect_->GetParameter(4)->SetValue(1.0f);   // Level 100%
        twelveStringEffect_->Update();
    }

#if MYSTERIOUS_WAH_V2
    if (wahEffect_ && wahEffect_->GetParameterCount() >= 8) {
        AutowahV2Effect* wah = static_cast<AutowahV2Effect*>(wahEffect_);
        wah->GetParameter(0)->SetValue(wahMix);
        wah->GetParameter(1)->SetValue(0.82f);     // Resonance
        wah->GetParameter(2)->SetValue(1200.0f);   // Frequency
        wah->GetParameter(3)->SetValue(0.09f);     // Attack
        wah->GetParameter(4)->SetValue(0.14f);     // Release
        wah->GetParameter(5)->SetValue(-sweepHz);  // Sensitivity
        wah->GetParameter(6)->SetValue(0.0f);      // Voice: "C Log"
        wah->GetParameter(7)->SetValue(downBoost); // Down boost
        wah->Update();
    }
#else
    if (wahEffect_ && wahEffect_->GetParameterCount() >= 6) {
        wahEffect_->GetParameter(0)->SetValue(wahMix);
        wahEffect_->GetParameter(1)->SetValue(0.82f);     // Resonance
        wahEffect_->GetParameter(2)->SetValue(1200.0f);   // Frequency
        wahEffect_->GetParameter(3)->SetValue(0.09f);     // Attack
        wahEffect_->GetParameter(4)->SetValue(0.14f);     // Release
        wahEffect_->GetParameter(5)->SetValue(-sweepHz);  // Sensitivity
        wahEffect_->Update();
    }
#endif

    if (flangerEffect_ && flangerEffect_->GetParameterCount() >= 6) {
        flangerEffect_->GetParameter(0)->SetValue(0.06f + (flangeAmount * 0.18f));
        flangerEffect_->GetParameter(1)->SetValue(0.10f + (flangeAmount * 0.32f));
        flangerEffect_->GetParameter(2)->SetValue(motionRate);
        flangerEffect_->GetParameter(3)->SetValue(0.01f + (flangeAmount * 0.08f));
        flangerEffect_->GetParameter(4)->SetValue(1.2f + (flangeAmount * 1.4f));
        flangerEffect_->GetParameter(5)->SetValue(0.0f);
        flangerEffect_->Update();
    }

    if (delayEffect_ && delayEffect_->GetParameterCount() >= 5) {
        delayEffect_->GetParameter(0)->SetValue(delayMix);
        delayEffect_->GetParameter(1)->SetValue(0.10f + (space * 0.70f));
        delayEffect_->GetParameter(2)->SetValue(3.0f);

        // Force the child delay to plain time mode (index 0 = Off) so it
        // never drifts into tempo sync; its own TimeParameter is linked to
        // this same toggle, so this also keeps that TimeParameter in TIME_MS.
        delayEffect_->GetParameter(3)->SetValue(delayTime->GetValueAsMs());
        delayEffect_->GetParameter(4)->SetValue(0.0f);

        delayEffect_->Update();
    }

    if (reverbEffect_) {
        reverbEffect_->SetMix(0.08f + (delayMix * 0.24f));
        reverbEffect_->SetFeedback(0.70f + (space * 0.22f));
        reverbEffect_->SetLpFreq(3400.0f - (space * 1500.0f));
        reverbEffect_->Update();
    }
}
