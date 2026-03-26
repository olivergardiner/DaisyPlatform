#include "mysteriouseffect.h"

#include "autowahv2effect.h"
#include "delayeffect.h"
#include "flangereffect.h"
#include "reverbeffect.h"
#include "../controls.h"
#include "../parameters/encoderparameter.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/timeparameter.h"
#include "../parameters/toggleparameter.h"

using namespace perspective;

namespace {

static const char* kDownBoostLabels[4] = {
    "Off", "Subtle", "Strong", "Max"
};

} // namespace

MysteriousEffect::MysteriousEffect()
    : CompoundEffect("Mysterious", RoutingMode::SERIES)
    , autowahV2Effect_(new AutowahV2Effect())
    , flangerEffect_(new FlangerEffect())
    , delayEffect_(new DelayEffect())
    , reverbEffect_(new ReverbEffect()) {
    AddEffect(autowahV2Effect_);
    AddEffect(flangerEffect_);
    AddEffect(delayEffect_);
    AddEffect(reverbEffect_);
}

MysteriousEffect::~MysteriousEffect() {
}

void MysteriousEffect::Init(float sampleRate) {
    CompoundEffect::Init(sampleRate);

    AddParameter(new PotentiometerParameter("K1 Wah Mix", 0.0f, 1.0f, 0.62f, PotCurve::LIN, KNOB_1_IDX, 0));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    AddParameter(new PotentiometerParameter("K2 Sweep", 300.0f, 2600.0f, 1100.0f, PotCurve::LOG, KNOB_2_IDX, 1));

    AddParameter(new PotentiometerParameter("K3 Flange", 0.0f, 1.0f, 0.20f, PotCurve::LIN, KNOB_3_IDX, 2));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    AddParameter(new PotentiometerParameter("K4 Motion", 0.05f, 0.8f, 0.14f, PotCurve::LOG, KNOB_4_IDX, 3));

    AddParameter(new PotentiometerParameter("K5 Echo", 0.0f, 0.65f, 0.22f, PotCurve::LIN, KNOB_5_IDX, 4));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    AddParameter(new PotentiometerParameter("K6 Space", 0.0f, 0.75f, 0.34f, PotCurve::LIN, KNOB_6_IDX, 5));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    AddParameter(new TimeParameter("E1 Time", 80.0f, 450.0f, 230.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo", 6));

    AddParameter(new EncoderParameter("E2 Down+", 0.0f, 3.0f, 2.0f, 1.0f, ENCODER_2_IDX, 7));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kDownBoostLabels, 4);

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

    if (autowahV2Effect_ && autowahV2Effect_->GetParameterCount() >= 8) {
        autowahV2Effect_->GetParameter(0)->SetValue(wahMix);
        autowahV2Effect_->GetParameter(1)->SetValue(0.82f);
        autowahV2Effect_->GetParameter(2)->SetValue(1200.0f);
        autowahV2Effect_->GetParameter(3)->SetValue(0.09f);
        autowahV2Effect_->GetParameter(4)->SetValue(0.14f);
        autowahV2Effect_->GetParameter(5)->SetValue(-sweepHz);
        autowahV2Effect_->GetParameter(6)->SetValue(0.0f);
        autowahV2Effect_->GetParameter(7)->SetValue(downBoost);
        autowahV2Effect_->Update();
    }

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

        EffectParameter* timeParam = delayEffect_->GetParameter(3);
        if (timeParam && timeParam->GetType() == ParameterType::ENCODER) {
            TimeParameter* delayTimeParam = static_cast<TimeParameter*>(timeParam);
            delayTimeParam->SetValue(delayTime->GetValueAsMs());
            delayTimeParam->SetDisplayMode(TimeDisplayMode::TIME_MS);
        }

        EffectParameter* tempoModeParam = delayEffect_->GetParameter(4);
        if (tempoModeParam && tempoModeParam->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam = static_cast<ToggleParameter*>(tempoModeParam);
            toggleParam->SetState(false);
        }

        delayEffect_->Update();
    }

    if (reverbEffect_) {
        reverbEffect_->SetMix(0.08f + (delayMix * 0.24f));
        reverbEffect_->SetFeedback(0.70f + (space * 0.22f));
        reverbEffect_->SetLpFreq(3400.0f - (space * 1500.0f));
        reverbEffect_->Update();
    }
}
