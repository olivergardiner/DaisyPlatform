#include "paralleldelayeffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"
#include "../parameters/timeparameter.h"

using namespace perspective;


ParallelDelayEffect::ParallelDelayEffect()
    : CompoundEffect("Parallel Delay", RoutingMode::PARALLEL)
    , delay1_(nullptr)
    , delay2_(nullptr)
{
    // Create two delay effects
    delay1_ = new DelayEffect();
    delay2_ = new DelayEffect();
    
    // Add them to the compound effect
    AddEffect(delay1_);
    AddEffect(delay2_);
}

ParallelDelayEffect::~ParallelDelayEffect() {
    // Destructor for CompoundEffect will delete child effects
}

void ParallelDelayEffect::Init(float sampleRate) {
    // Initialize the compound effect (this initializes child effects)
    CompoundEffect::Init(sampleRate);
    
    // Configure parallel routing: pass clean signal, no scaling, delays are wet only
    SetPassClean(true);
    SetScaleParallel(false);
    if (delay1_) delay1_->SetWetOnly(true);
    if (delay2_) delay2_->SetWetOnly(true);
    
    // Add parameters for Delay 1
    auto* mix1Param = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.50f);
    mix1Param->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mix1Param->SetDisplayType(DisplayType::SCALED);
    mix1Param->SetScaleFactor(100.0f); // Display mix as percentage
    mix1Param->SetMacroRole(MacroRole::MIX);
    AddParameter(mix1Param);

    auto* feedback1Param = new ValueParameter("K4 Feedback", 0.0f, 0.95f, 0.5f);
    feedback1Param->BindPotentiometer(MACRO_KNOB_FEEDBACK_IDX, PotCurve::LIN);
    feedback1Param->SetDisplayType(DisplayType::SCALED);
    feedback1Param->SetScaleFactor(100.0f); // Display feedback as percentage
    feedback1Param->SetMacroRole(MacroRole::FEEDBACK);
    AddParameter(feedback1Param);

    auto* subdivision1Param = new EnumParameter("K5 Subdivision 1", TempoEffect::kSubdivisionGlyphs, 8, 3);
    subdivision1Param->BindPotentiometer(MACRO_KNOB_SUBDIVISION_IDX);
    subdivision1Param->SetMacroRole(MacroRole::SUBDIVISION);
    AddParameter(subdivision1Param);

    // Add parameters for Delay 2 (second instance - not macro-pot controllable)
    auto* mix2Param = new ValueParameter("Mix 2", 0.0f, 1.0f, 0.50f);
    mix2Param->SetDisplayType(DisplayType::SCALED);
    mix2Param->SetScaleFactor(100.0f); // Display mix as percentage
    mix2Param->SetMacroRole(MacroRole::MIX, /*isPrimary=*/false);
    AddParameter(mix2Param);

    auto* feedback2Param = new ValueParameter("Feedback 2", 0.0f, 0.95f, 0.5f);
    feedback2Param->SetDisplayType(DisplayType::SCALED);
    feedback2Param->SetScaleFactor(100.0f); // Display feedback as percentage
    feedback2Param->SetMacroRole(MacroRole::FEEDBACK, /*isPrimary=*/false);
    AddParameter(feedback2Param);

    auto* subdivision2Param = new EnumParameter("K6 Subdivision 2", TempoEffect::kSubdivisionGlyphs, 8, 3);
    subdivision2Param->BindPotentiometer(KNOB_6_IDX);
    subdivision2Param->SetMacroRole(MacroRole::SUBDIVISION, /*isPrimary=*/false);
    AddParameter(subdivision2Param);

    // Add delay 1 time parameter (Encoder 1) - TimeParameter with ms range (250-2000 ms = 240-30 BPM)
    auto* timeParam1 = new TimeParameter("E1 Time 1", 250.0f, 2000.0f, 500.0f, "E1 Tempo 1");
    timeParam1->BindEncoder(ENCODER_1_IDX, 1.0f);
    AddParameter(timeParam1);

    // Add delay 2 time parameter (Encoder 2) - TimeParameter with ms range (250-2000 ms = 240-30 BPM)
    auto* timeParam2 = new TimeParameter("E2 Time 2", 250.0f, 2000.0f, 500.0f, "E2 Tempo 2");
    timeParam2->BindEncoder(ENCODER_2_IDX, 1.0f);
    AddParameter(timeParam2);

    // Add delay 1 tempo mode toggle (Encoder 1 button)
    auto* tempoMode1Param = new EnumParameter("Tempo Mode", {"Off", "On"}, 0, -1);  // Hidden - tempo mode toggle
    tempoMode1Param->BindButton(ENCODER_1_BUTTON_IDX);
    AddParameter(tempoMode1Param);
    timeParam1->SetModeToggle(tempoMode1Param);

    // Add delay 2 tempo mode toggle (Encoder 2 button)
    auto* tempoMode2Param = new EnumParameter("Tempo Mode", {"Off", "On"}, 0, -1);  // Hidden - tempo mode toggle
    tempoMode2Param->BindButton(ENCODER_2_BUTTON_IDX);
    AddParameter(tempoMode2Param);
    timeParam2->SetModeToggle(tempoMode2Param);

    // Note: Metronome is now controlled globally by Perspective via SetMetronomeEnabled()
    
    // Set default parameters
    Update();
}

void ParallelDelayEffect::Update() {
    // Don't call CompoundEffect::Update() - we manually manage child parameters below
    // to avoid double-updating the child delays which causes artifacts
    
    // Update our custom parameters into the child delay effects
    if (parameters_.size() >= 10 && delay1_ && delay2_) {
        // Get parameter values from our parameters
        float mix1 = parameters_[kParamD1Mix]->GetValue();
        float feedback1 = parameters_[kParamD1Feedback]->GetValue();
        float subdivision1 = parameters_[kParamD1Subdivision]->GetValue();
        float mix2 = parameters_[kParamD2Mix]->GetValue();
        float feedback2 = parameters_[kParamD2Feedback]->GetValue();
        float subdivision2 = parameters_[kParamD2Subdivision]->GetValue();
        
        // Delay 1 time parameter (index 6) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam1 = static_cast<TimeParameter*>(parameters_[kParamD1Time]);
        float delayTime1 = timeParam1->GetValue();
        
        // Delay 2 time parameter (index 7) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam2 = static_cast<TimeParameter*>(parameters_[kParamD2Time]);
        float delayTime2 = timeParam2->GetValue();
        
        // Delay 1 tempo mode comes from the linked mode toggle (index 8)
        bool newTempoMode1 = timeParam1->GetDisplayMode() == TimeDisplayMode::TEMPO_BPM;
        if (newTempoMode1 != tempoMode1_) {
            tempoMode1_ = newTempoMode1;
            // Request display update since the parameter name changed
            RequestParameterDisplayUpdate(kParamD1Time); // Index 6 is delay 1 time parameter

            // Force update of delay 1 when tempo mode changes
            lastDelayTime1_ = -1.0f; // Force recalculation
        }

        // Delay 2 tempo mode comes from the linked mode toggle (index 9)
        bool newTempoMode2 = timeParam2->GetDisplayMode() == TimeDisplayMode::TEMPO_BPM;
        if (newTempoMode2 != tempoMode2_) {
            tempoMode2_ = newTempoMode2;
            // Request display update since the parameter name changed
            RequestParameterDisplayUpdate(kParamD2Time); // Index 7 is delay 2 time parameter

            // Force update of delay 2 when tempo mode changes
            lastDelayTime2_ = -1.0f; // Force recalculation
        }
        
        // Update Delay 1 parameters only if they've changed
        // DelayEffect parameter indices: 0=Mix, 1=Feedback, 2=Subdivision, 3=Time/BPM, 4=TempoToggle
        bool delay1Changed = false;
        if (delay1_->GetParameterCount() >= 5) {
            if (mix1 != lastMix1_) {
                delay1_->GetParameter(0)->SetValue(mix1);
                lastMix1_ = mix1;
                delay1Changed = true;
            }
            if (feedback1 != lastFeedback1_) {
                delay1_->GetParameter(1)->SetValue(feedback1);
                lastFeedback1_ = feedback1;
                delay1Changed = true;
            }
            if (subdivision1 != lastSubdivision1_) {
                delay1_->GetParameter(2)->SetValue(subdivision1);
                lastSubdivision1_ = subdivision1;
                delay1Changed = true;
            }
            if (delayTime1 != lastDelayTime1_) {
                delay1_->GetParameter(3)->SetValue(delayTime1);
                lastDelayTime1_ = delayTime1;
                delay1Changed = true;
            }
            float tempoModeValue1 = tempoMode1_ ? 1.0f : 0.0f;
            delay1_->GetParameter(4)->SetValue(tempoModeValue1);
        }
        
        // Update Delay 2 parameters only if they've changed
        bool delay2Changed = false;
        if (delay2_->GetParameterCount() >= 5) {
            if (mix2 != lastMix2_) {
                delay2_->GetParameter(0)->SetValue(mix2);
                lastMix2_ = mix2;
                delay2Changed = true;
            }
            if (feedback2 != lastFeedback2_) {
                delay2_->GetParameter(1)->SetValue(feedback2);
                lastFeedback2_ = feedback2;
                delay2Changed = true;
            }
            if (subdivision2 != lastSubdivision2_) {
                delay2_->GetParameter(2)->SetValue(subdivision2);
                lastSubdivision2_ = subdivision2;
                delay2Changed = true;
            }
            if (delayTime2 != lastDelayTime2_) {
                delay2_->GetParameter(3)->SetValue(delayTime2);
                lastDelayTime2_ = delayTime2;
                delay2Changed = true;
            }
            float tempoModeValue2 = tempoMode2_ ? 1.0f : 0.0f;
            delay2_->GetParameter(4)->SetValue(tempoModeValue2);
        }
        
        // Only call Update() on child delays if their parameters actually changed
        if (delay1Changed) {
            delay1_->Update();
        }
        if (delay2Changed) {
            delay2_->Update();
        }
    }
}
