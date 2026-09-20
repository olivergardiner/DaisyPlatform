#include "flyeffect.h"
#include "autowahv2effect.h"
#include "delayeffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"
#include "../parameters/timeparameter.h"

using namespace perspective;

FlyEffect::FlyEffect()
    : CompoundEffect("Fly", RoutingMode::SERIES)
    , autowahV2Effect_(nullptr)
    , delayEffect_(nullptr)
    , timeParamIndex_(-1)
    , tempoModeParamIndex_(-1) {
}

FlyEffect::~FlyEffect() {
    // Effects are cleaned up by CompoundEffect destructor
}

void FlyEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Create and add AutoWahV2 effect
    autowahV2Effect_ = new AutowahV2Effect();
    AddEffect(autowahV2Effect_);
    
    // Create and add DelayEffect
    delayEffect_ = new DelayEffect();
    AddEffect(delayEffect_);
    
    // Call parent Init to initialize all child effects
    CompoundEffect::Init(sampleRate);
    
    // Top-level macros tuned for external drive before the loop.
    auto* wahMixParam = new ValueParameter("K1 Wah Mix", 0.0f, 1.0f, 0.88f);
    wahMixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    wahMixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(wahMixParam);

    AddParameter(new ValueParameter("Sweep Hz", 250.0f, 1600.0f, 680.0f));
    AddParameter(new ValueParameter("Sens", 300.0f, 2200.0f, 1200.0f));

    auto* echoMixParam = new ValueParameter("Echo Mix", 0.0f, 0.8f, 0.58f);
    echoMixParam->SetMacroRole(MacroRole::MIX, /*isPrimary=*/false);
    AddParameter(echoMixParam);

    AddParameter(new ValueParameter("Echo Fdbk", 0.0f, 0.65f, 0.26f));

    // Add E1 Time/Tempo parameter (defaults to 556ms ≈ 108 BPM)
    // TimeParameter with milliseconds range (10-2000 ms), 1ms step in time mode, 0.5 BPM in tempo mode
    TimeParameter* timeParam = new TimeParameter("E1 Time", 10.0f, 2000.0f, 556.0f, "E1 Tempo");
    timeParam->BindEncoder(ENCODER_1_IDX, 1.0f, /*reversed=*/true);
    AddParameter(timeParam);
    timeParamIndex_ = 5;

    // Add E1 Button toggle for Tempo Mode (starts in tempo mode = index 1)
    auto* tempoModeParam = new EnumParameter("Tempo Mode", {"Time", "Tempo"}, 1, -1);
    tempoModeParam->BindButton(ENCODER_1_BUTTON_IDX);
    AddParameter(tempoModeParam);
    // TimeParameter's display mode derives from this toggle, so linking it
    // also puts timeParam in tempo mode immediately (toggle defaults to index 1).
    timeParam->SetModeToggle(tempoModeParam);
    tempoModeParamIndex_ = 6;

    // Set subdivision parameter on delay effect to 3/16ths (index 2)
    // The delay effect has its own parameter indices, so we need to access it directly
    if (delayEffect_ && delayEffect_->GetParameterCount() >= 3) {
        // DelayEffect has: [0]=Mix, [1]=Feedback, [2]=Subdivision, [3]=Time, [4]=TempoToggle
        // Set subdivision to index 2 (3/16ths)
        EffectParameter* subdivParam = delayEffect_->GetParameter(2);
        if (subdivParam && subdivParam->GetKind() == ParameterKind::ENUM) {
            static_cast<EnumParameter*>(subdivParam)->SetSelectedIndex(2);
        }
    }

    // Set tempo mode on delay effect. The child DelayEffect's own TimeParameter
    // is already linked to its own tempo mode toggle (index 4) internally, so
    // setting that toggle here is enough to put the child in tempo mode too.
    if (delayEffect_ && delayEffect_->GetParameterCount() >= 5) {
        EffectParameter* childTempoModeParam = delayEffect_->GetParameter(4);
        if (childTempoModeParam && childTempoModeParam->GetKind() == ParameterKind::ENUM) {
            static_cast<EnumParameter*>(childTempoModeParam)->SetSelectedIndex(1);
        }
    }

    Update();
}

void FlyEffect::Update() {
    TimeParameter* flyTimeParam = nullptr;
    EnumParameter* flyTempoModeParam = nullptr;
    float tempoHz = 2.0f;

    if (timeParamIndex_ >= 0 && timeParamIndex_ < static_cast<int>(GetParameterCount()) &&
        tempoModeParamIndex_ >= 0 && tempoModeParamIndex_ < static_cast<int>(GetParameterCount())) {
        flyTimeParam = static_cast<TimeParameter*>(GetParameter(timeParamIndex_));
        flyTempoModeParam = static_cast<EnumParameter*>(GetParameter(tempoModeParamIndex_));
    }

    if (GetParameterCount() >= 7) {
        float wahMix = GetParameter(0)->GetValue();
        float sweepHz = GetParameter(1)->GetValue();
        float sensitivity = GetParameter(2)->GetValue();
        float delayMix = GetParameter(3)->GetValue();
        float delayFeedback = GetParameter(4)->GetValue();

        // Shape wah for the "Fly" pulse while leaving gain staging to external drive.
        if (autowahV2Effect_ && autowahV2Effect_->GetParameterCount() >= 8) {
            autowahV2Effect_->GetParameter(0)->SetValue(wahMix);
            autowahV2Effect_->GetParameter(1)->SetValue(0.90f);
            autowahV2Effect_->GetParameter(2)->SetValue(sweepHz);
            autowahV2Effect_->GetParameter(3)->SetValue(0.07f);
            autowahV2Effect_->GetParameter(4)->SetValue(0.05f);
            autowahV2Effect_->GetParameter(5)->SetValue(sensitivity);
            autowahV2Effect_->GetParameter(6)->SetValue(0.0f);
            autowahV2Effect_->GetParameter(7)->SetValue(1.0f);
        }

        if (delayEffect_ && delayEffect_->GetParameterCount() >= 5) {
            // Keep repeats audible even when front-panel knobs are near minimum.
            float mappedMix = 0.28f + (delayMix * 0.52f);
            float mappedFeedback = 0.16f + (delayFeedback * 0.42f);

            delayEffect_->GetParameter(0)->SetValue(mappedMix);
            delayEffect_->GetParameter(1)->SetValue(mappedFeedback);
            delayEffect_->GetParameter(2)->SetValue(2.0f); // 3/16 subdivision

            if (flyTimeParam && delayEffect_->GetParameterCount() >= 4) {
                delayEffect_->GetParameter(3)->SetValue(flyTimeParam->GetValueAsMs());
            }

            if (flyTempoModeParam && delayEffect_->GetParameterCount() >= 5) {
                // Both are 2-option enums with the same Off/On (0/1) encoding.
                delayEffect_->GetParameter(4)->SetValue(flyTempoModeParam->GetValue());
            }
        }
    }

    // Handle tempo mode toggle from E1 Button. TimeParameter's display mode
    // is derived live from the linked toggle, so just detect the change to
    // refresh the visible E1 display.
    if (flyTimeParam && flyTempoModeParam) {
        bool tempoOn = flyTimeParam->GetDisplayMode() == TimeDisplayMode::TEMPO_BPM;
        if (tempoOn != tempoModeCached_) {
            tempoModeCached_ = tempoOn;
            RequestParameterDisplayUpdate(timeParamIndex_);
        }

        // Convert BPM to Hz for SetTempo (SetTempo expects Hz)
        float bpm = flyTimeParam->GetValueAsBPM();
        tempoHz = bpm / 60.0f;
    }

    SetTempo(tempoHz);
    
    // Call parent Update to update child effects
    CompoundEffect::Update();
}

void FlyEffect::SetTempo(float tempoHz) {
    // Tempo should only drive delay timing in Fly.
    tempo_ = tempoHz;
    if (delayEffect_) {
        delayEffect_->SetTempo(tempoHz);
    }
}
