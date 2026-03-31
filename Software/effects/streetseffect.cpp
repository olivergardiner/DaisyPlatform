#include "streetseffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/timeparameter.h"
#include "../parameters/toggleparameter.h"

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
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_1_IDX, 0));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_2_IDX, 1));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    AddParameter(new PotentiometerParameter("K3 Subdivision", 0.0f, 7.0f, 2.0f, PotCurve::LIN, KNOB_3_IDX, 2));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(TempoEffect::kSubdivisionGlyphs, 8);
    
    // Delay 2 controls
    AddParameter(new PotentiometerParameter("K4 Mix", 0.0f, 1.0f, 0.35f, PotCurve::LIN, KNOB_4_IDX, 4));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    AddParameter(new PotentiometerParameter("K5 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_5_IDX, 5));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);
    
    // Slapback blend
    AddParameter(new PotentiometerParameter("K6 Slap Mix", 0.0f, 0.75f, 0.50f, PotCurve::LIN, KNOB_6_IDX, 6));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    // Delay 1 time parameter (Encoder 1) — reversed so CW increases BPM / decreases delay time
    AddParameter(new TimeParameter("E1 Time", 250.0f, 2000.0f, 500.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo", 3));
    static_cast<EncoderParameter*>(parameters_.back())->SetReversed(true);
    
    // Delay 2 time parameter (Encoder 2)
    AddParameter(new TimeParameter("E2 Perc", 250.0f, 2000.0f, 510.0f, 1.0f, ENCODER_2_IDX, "E2 Perc", 7));
    
    // Delay 1 tempo mode toggle (Encoder 1 button)
    AddParameter(new ToggleParameter("Tempo Mode", false, ENCODER_1_BUTTON_IDX, "On", "Off",-1));  // Hidden - tempo mode toggle

    // Initialize startup timing modes/values:
    // Delay 1: tempo mode at reference BPM.
    parameters_[kParamTempoMode]->SetValue(1.0f);
    parameters_[kParamTime1]->SetValue(60000.0f / kReferenceBpm);

    // Delay 2 starts at 510ms.
    parameters_[kParamTime2]->SetValue(510.0f);

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
        if (parameters_[kParamTime1]->GetType() != ParameterType::ENCODER ||
            parameters_[kParamTime2]->GetType() != ParameterType::ENCODER ||
            parameters_[kParamTempoMode]->GetType() != ParameterType::TOGGLE) {
            return;
        }

        TimeParameter* timeParam1 = static_cast<TimeParameter*>(parameters_[kParamTime1]);
        TimeParameter* timeParam2 = static_cast<TimeParameter*>(parameters_[kParamTime2]);
        ToggleParameter* tempoToggle1 = static_cast<ToggleParameter*>(parameters_[kParamTempoMode]);

        // Delay 1: pass the raw beat period to the child delay so it can apply
        // subdivision itself — this keeps the metronome at the correct BPM.
        // (Pre-multiplying by kPrimarySubdivision here caused the child delay to
        // double-apply subdivision, making the metronome run 33% too fast.)
        float beatPeriodMs = tempoToggle1->GetState()
            ? (60000.0f / timeParam1->GetValueAsBPM())
            : timeParam1->GetValueAsMs();
        beatPeriodMs = std::max(80.0f, std::min(beatPeriodMs, 2000.0f));

        // Delay 2 is independent — use its own time parameter directly.
        float delay2Ms = timeParam2->GetValueAsMs();

        // Explicitly map streets controls into parallel delay controls.
        parallelDelay_->GetParameter(0)->SetValue(parameters_[kParamMix]->GetValue());
        parallelDelay_->GetParameter(1)->SetValue(parameters_[kParamFeedback]->GetValue());
        parallelDelay_->GetParameter(2)->SetValue(tempoToggle1->GetState() ? 2.0f : parameters_[kParamSubdivision]->GetValue());
        parallelDelay_->GetParameter(3)->SetValue(parameters_[kParamMix2]->GetValue());
        parallelDelay_->GetParameter(4)->SetValue(parameters_[kParamFeedback2]->GetValue());
        parallelDelay_->GetParameter(5)->SetValue(3.0f);    // fixed quarter multiplier in time mode
        parallelDelay_->GetParameter(6)->SetValue(beatPeriodMs); // raw beat period; child applies subdivision
        parallelDelay_->GetParameter(7)->SetValue(delay2Ms);
        parallelDelay_->GetParameter(8)->SetValue(tempoToggle1->GetState() ? 1.0f : 0.0f);
        parallelDelay_->GetParameter(9)->SetValue(0.0f);    // Delay 2 always time mode
        
        // Detect tempo mode changes for Delay 1 (toggle index 8) and update our own TimeParameter
        bool newTempoMode1 = tempoToggle1->GetState();
        if (newTempoMode1 != tempoMode1_) {
            tempoMode1_ = newTempoMode1;
            timeParam1->SetDisplayMode(tempoMode1_ ? TimeDisplayMode::TEMPO_BPM : TimeDisplayMode::TIME_MS);
            RequestParameterDisplayUpdate(6);
        }

        // Delay 2 is always linked milliseconds (not tempo-mode user-controlled).
        timeParam2->SetDisplayMode(TimeDisplayMode::TIME_MS);
        
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
