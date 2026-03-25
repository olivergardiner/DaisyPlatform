#include "flyeffect.h"
#include "autowahv2effect.h"
#include "delayeffect.h"
#include "../controls.h"
#include "../parameters/timeparameter.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/toggleparameter.h"

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
    AddParameter(new PotentiometerParameter("K1 Wah Mix", 0.0f, 1.0f, 0.88f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Sweep Hz", 250.0f, 1600.0f, 680.0f, PotCurve::LOG, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Sens", 300.0f, 2200.0f, 1200.0f, PotCurve::LIN, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("K4 Echo Mix", 0.0f, 0.8f, 0.58f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("K5 Echo Fdbk", 0.0f, 0.65f, 0.26f, PotCurve::LIN, KNOB_5_IDX));

    // Add E1 Time/Tempo parameter (defaults to 556ms ≈ 108 BPM)
    // TimeParameter with milliseconds range (10-2000 ms), 1ms step in time mode, 0.5 BPM in tempo mode
    TimeParameter* timeParam = new TimeParameter("E1 Time", 10.0f, 2000.0f, 556.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo");
    // Start in tempo mode (true)
    timeParam->SetDisplayMode(TimeDisplayMode::TEMPO_BPM);
    AddParameter(timeParam);
    timeParamIndex_ = 5;
    
    // Add E1 Button toggle for Tempo Mode (starts in tempo mode = true)
    AddParameter(new ToggleParameter("Tempo Mode", true, ENCODER_1_BUTTON_IDX, "Tempo", "Time", -1));
    tempoModeParamIndex_ = 6;
    
    // Set subdivision parameter on delay effect to 3/16ths (index 2)
    // The delay effect has its own parameter indices, so we need to access it directly
    if (delayEffect_ && delayEffect_->GetParameterCount() >= 3) {
        // DelayEffect has: [0]=Mix, [1]=Feedback, [2]=Subdivision, [3]=Time, [4]=TempoToggle
        // Set subdivision to index 2 (3/16ths)
        EffectParameter* subdivParam = delayEffect_->GetParameter(2);
        if (subdivParam && subdivParam->GetType() == ParameterType::POTENTIOMETER) {
            PotentiometerParameter* potParam = static_cast<PotentiometerParameter*>(subdivParam);
            // Set to 2.0 out of 6.0 max = 33.33% normalized value
            potParam->SetNormalizedValueWithCurve(2.0f / 6.0f);
        }
    }

    // Set tempo mode on delay effect
    if (delayEffect_ && delayEffect_->GetParameterCount() >= 5) {
        // DelayEffect tempo mode toggle is at index 4
        EffectParameter* tempoModeParam = delayEffect_->GetParameter(4);
        if (tempoModeParam && tempoModeParam->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam = static_cast<ToggleParameter*>(tempoModeParam);
            // Set to tempo mode (true)
            toggleParam->SetState(true);
        }
        
        // Also set the DelayEffect's TimeParameter to tempo mode
        if (delayEffect_->GetParameterCount() >= 4) {
            EffectParameter* timeParam = delayEffect_->GetParameter(3);
            if (timeParam && timeParam->GetType() == ParameterType::ENCODER) {
                TimeParameter* delayTimeParam = static_cast<TimeParameter*>(timeParam);
                delayTimeParam->SetDisplayMode(TimeDisplayMode::TEMPO_BPM);
            }
        }
    }

    Update();
}

void FlyEffect::Update() {
    TimeParameter* flyTimeParam = nullptr;
    ToggleParameter* flyTempoModeParam = nullptr;
    float tempoHz = 2.0f;

    if (timeParamIndex_ >= 0 && timeParamIndex_ < static_cast<int>(GetParameterCount()) &&
        tempoModeParamIndex_ >= 0 && tempoModeParamIndex_ < static_cast<int>(GetParameterCount())) {
        flyTimeParam = static_cast<TimeParameter*>(GetParameter(timeParamIndex_));
        flyTempoModeParam = static_cast<ToggleParameter*>(GetParameter(tempoModeParamIndex_));
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
                EffectParameter* delayTimeParam = delayEffect_->GetParameter(3);
                if (delayTimeParam && delayTimeParam->GetType() == ParameterType::ENCODER) {
                    TimeParameter* childTime = static_cast<TimeParameter*>(delayTimeParam);
                    childTime->SetValue(flyTimeParam->GetValueAsMs());
                }
            }

            if (flyTempoModeParam && delayEffect_->GetParameterCount() >= 5) {
                EffectParameter* delayTempoModeParam = delayEffect_->GetParameter(4);
                if (delayTempoModeParam && delayTempoModeParam->GetType() == ParameterType::TOGGLE) {
                    ToggleParameter* childTempoToggle = static_cast<ToggleParameter*>(delayTempoModeParam);
                    childTempoToggle->SetState(flyTempoModeParam->GetState());
                }
            }
        }
    }

    // Handle tempo mode toggle from E1 Button
    if (flyTimeParam && flyTempoModeParam) {
        TimeParameter* timeParam = flyTimeParam;
        ToggleParameter* tempoModeParam = flyTempoModeParam;
        
        // Update TimeParameter's display mode based on toggle state
        TimeDisplayMode newMode = tempoModeParam->GetState() ? TimeDisplayMode::TEMPO_BPM : TimeDisplayMode::TIME_MS;
        if (timeParam->GetDisplayMode() != newMode) {
            timeParam->SetDisplayMode(newMode);
            // Ensure the visible E1 parameter display updates when mode toggles.
            RequestParameterDisplayUpdate(timeParamIndex_);
        }
        
        // Convert BPM to Hz for SetTempo (SetTempo expects Hz)
        float bpm = timeParam->GetValueAsBPM();
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
