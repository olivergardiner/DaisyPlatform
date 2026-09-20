#include "streetseffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"
#include "../parameters/timeparameter.h"

#include <algorithm>
#include <cmath>

using namespace perspective;


StreetsEffect::StreetsEffect()
    : CompoundEffect("Streets", RoutingMode::SERIES)
    , parallelDelay_(nullptr)
    , slapbackDelay_(nullptr)
{
    // Create parallel delay followed by slapback delay in series
    parallelDelay_ = new ParallelDelayEffect();
    slapbackDelay_ = new SlapbackDelayEffect();
    
    // Add them to the compound effect (in series)
    AddEffect(parallelDelay_);
    AddEffect(slapbackDelay_);
}

StreetsEffect::~StreetsEffect() {
    // CompoundEffect destructor will delete child effects
}

void StreetsEffect::Init(float sampleRate) {
    // Initialize the compound effect (this initializes child effects)
    CompoundEffect::Init(sampleRate);
    
    // Set slapback delay to be wet only so it processes the parallel delay output
    if (slapbackDelay_) {
        slapbackDelay_->SetWetOnly(false); // Allow slapback to add to the signal
    }
    
    // Add top-level controls.
    // Delay 1 controls
    auto* mixParam = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.50f, 0);
    mixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mixParam->SetDisplayType(DisplayType::SCALED);
    mixParam->SetScaleFactor(100.0f);
    mixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(mixParam);

    auto* feedbackParam = new ValueParameter("K4 Feedback", 0.0f, 0.95f, 0.5f, 1);
    feedbackParam->BindPotentiometer(MACRO_KNOB_FEEDBACK_IDX, PotCurve::LIN);
    feedbackParam->SetDisplayType(DisplayType::SCALED);
    feedbackParam->SetScaleFactor(100.0f);
    feedbackParam->SetMacroRole(MacroRole::FEEDBACK);
    AddParameter(feedbackParam);

    auto* subdivisionParam = new EnumParameter("K5 Subdivision 1", TempoEffect::kSubdivisionGlyphs, 8, 2, 2);
    subdivisionParam->BindPotentiometer(MACRO_KNOB_SUBDIVISION_IDX);
    subdivisionParam->SetMacroRole(MacroRole::SUBDIVISION);
    AddParameter(subdivisionParam);

    // Delay 2 controls (second instance - not macro-pot controllable)
    auto* mix2Param = new ValueParameter("Mix 2", 0.0f, 1.0f, 0.35f, 4);
    mix2Param->SetDisplayType(DisplayType::SCALED);
    mix2Param->SetScaleFactor(100.0f);
    mix2Param->SetMacroRole(MacroRole::MIX, /*isPrimary=*/false);
    AddParameter(mix2Param);

    auto* feedback2Param = new ValueParameter("Feedback 2", 0.0f, 0.95f, 0.5f, 5);
    feedback2Param->SetDisplayType(DisplayType::SCALED);
    feedback2Param->SetScaleFactor(100.0f);
    feedback2Param->SetMacroRole(MacroRole::FEEDBACK, /*isPrimary=*/false);
    AddParameter(feedback2Param);

    // Slapback blend
    auto* slapMixParam = new ValueParameter("K6 Slap Mix", 0.0f, 0.75f, 0.50f, 6);
    slapMixParam->BindPotentiometer(KNOB_6_IDX, PotCurve::LIN);
    slapMixParam->SetDisplayType(DisplayType::SCALED);
    slapMixParam->SetScaleFactor(100.0f);
    slapMixParam->SetMacroRole(MacroRole::MIX, /*isPrimary=*/false);
    AddParameter(slapMixParam);

    // Delay 1 time parameter (Encoder 1) — reversed so CW increases BPM / decreases delay time
    auto* timeParam1 = new TimeParameter("E1 Time", 250.0f, 2000.0f, 500.0f, "E1 Tempo", 3);
    timeParam1->BindEncoder(ENCODER_1_IDX, 1.0f, /*reversed=*/true);
    AddParameter(timeParam1);

    // Delay 2 time parameter (Encoder 2)
    auto* timeParam2 = new TimeParameter("E2 Perc", 250.0f, 2000.0f, 510.0f, "E2 Perc", 7);
    timeParam2->BindEncoder(ENCODER_2_IDX, 1.0f);
    AddParameter(timeParam2);

    // Delay 1 tempo mode toggle (Encoder 1 button)
    auto* tempoModeParam = new EnumParameter("Tempo Mode", {"Off", "On"}, 0, -1);  // Hidden - tempo mode toggle
    tempoModeParam->BindButton(ENCODER_1_BUTTON_IDX);
    AddParameter(tempoModeParam);
    timeParam1->SetModeToggle(tempoModeParam);

    // Initialize startup timing modes/values:
    // Delay 1: tempo mode at reference BPM.
    tempoModeParam->SetSelectedIndex(1);
    timeParam1->SetValue(60000.0f / kReferenceBpm);

    // Delay 2 starts at 510ms.
    timeParam2->SetValue(510.0f);

    // Note: Metronome is now controlled globally by Perspective via SetMetronomeEnabled()
    
    // Set slapback delay time to fixed 50ms
    if (slapbackDelay_ && slapbackDelay_->GetParameterCount() >= 3) {
        // Set Time parameter (index 2) to 50ms
        slapbackDelay_->GetParameter(2)->SetValue(50.0f);
        // Set Mix to 50% (index 0)
        slapbackDelay_->GetParameter(0)->SetValue(0.5f);
        // Set Feedback to 25% (index 1)
        slapbackDelay_->GetParameter(1)->SetValue(0.25f);
    }
    
    Update();
}

void StreetsEffect::Update() {
    // Update parallel delay parameters from our parameters
    if (parameters_.size() >= 9 && parallelDelay_ && parallelDelay_->GetParameterCount() >= 10) {
        if (parameters_[kParamTime1]->GetKind() != ParameterKind::TIME ||
            parameters_[kParamTime2]->GetKind() != ParameterKind::TIME ||
            parameters_[kParamTempoMode]->GetKind() != ParameterKind::ENUM) {
            return;
        }

        TimeParameter* timeParam1 = static_cast<TimeParameter*>(parameters_[kParamTime1]);
        TimeParameter* timeParam2 = static_cast<TimeParameter*>(parameters_[kParamTime2]);
        bool tempoOn = timeParam1->GetDisplayMode() == TimeDisplayMode::TEMPO_BPM;

        // Delay 1: pass the raw beat period to the child delay so it can apply
        // subdivision itself — this keeps the metronome at the correct BPM.
        // (Pre-multiplying by kPrimarySubdivision here caused the child delay to
        // double-apply subdivision, making the metronome run 33% too fast.)
        float beatPeriodMs = tempoOn
            ? (60000.0f / timeParam1->GetValueAsBPM())
            : timeParam1->GetValueAsMs();
        beatPeriodMs = std::max(80.0f, std::min(beatPeriodMs, 2000.0f));

        // Delay 2 is independent — use its own time parameter directly.
        float delay2Ms = timeParam2->GetValueAsMs();

        // Explicitly map streets controls into parallel delay controls.
        parallelDelay_->GetParameter(0)->SetValue(parameters_[kParamMix]->GetValue());
        parallelDelay_->GetParameter(1)->SetValue(parameters_[kParamFeedback]->GetValue());
        parallelDelay_->GetParameter(2)->SetValue(tempoOn ? 2.0f : parameters_[kParamSubdivision]->GetValue());
        parallelDelay_->GetParameter(3)->SetValue(parameters_[kParamMix2]->GetValue());
        parallelDelay_->GetParameter(4)->SetValue(parameters_[kParamFeedback2]->GetValue());
        parallelDelay_->GetParameter(5)->SetValue(3.0f);    // fixed quarter multiplier in time mode
        parallelDelay_->GetParameter(6)->SetValue(beatPeriodMs); // raw beat period; child applies subdivision
        parallelDelay_->GetParameter(7)->SetValue(delay2Ms);
        parallelDelay_->GetParameter(8)->SetValue(tempoOn ? 1.0f : 0.0f);
        parallelDelay_->GetParameter(9)->SetValue(0.0f);    // Delay 2 always time mode

        // Detect tempo mode changes for Delay 1 (toggle index 8) and update our own TimeParameter
        if (tempoOn != tempoMode1_) {
            tempoMode1_ = tempoOn;
            RequestParameterDisplayUpdate(kParamTime1);
        }

        // Delay 2 has no mode toggle linked, so it always reads as TIME_MS.

        // Update the parallel delay
        parallelDelay_->Update();
    }
    
    // Update the slapback delay (keep it at 50ms)
    if (slapbackDelay_) {
        if (slapbackDelay_->GetParameterCount() >= 1 && parameters_.size() > 5) {
            slapbackDelay_->GetParameter(0)->SetValue(parameters_[kParamSlapMix]->GetValue());
        }
        slapbackDelay_->Update();
    }
}
