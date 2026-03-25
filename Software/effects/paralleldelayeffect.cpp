#include "paralleldelayeffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/encoderparameter.h"
#include "../parameters/timeparameter.h"
#include "../parameters/toggleparameter.h"

using namespace perspective;

static const char *subDivisions[7] = {
    "_", "^", "^ `", "]", "] ` `", "] `", "\\"
};

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
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display mix as percentage
    
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display feedback as percentage
    
    AddParameter(new PotentiometerParameter("K3 Subdivision", 0.0f, 6.0f, 3.0f, PotCurve::LIN, KNOB_3_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(subDivisions, 7);
    
    // Add parameters for Delay 2
    AddParameter(new PotentiometerParameter("K4 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_4_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display mix as percentage
    
    AddParameter(new PotentiometerParameter("K5 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_5_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display feedback as percentage
    
    AddParameter(new PotentiometerParameter("K6 Subdivision", 0.0f, 6.0f, 3.0f, PotCurve::LIN, KNOB_6_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(subDivisions, 7);
    
    // Add delay 1 time parameter (Encoder 1) - TimeParameter with ms range (250-2000 ms = 240-30 BPM)
    AddParameter(new TimeParameter("E1 Time 1", 250.0f, 2000.0f, 500.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo 1"));
    
    // Add delay 2 time parameter (Encoder 2) - TimeParameter with ms range (250-2000 ms = 240-30 BPM)
    AddParameter(new TimeParameter("E2 Time 2", 250.0f, 2000.0f, 500.0f, 1.0f, ENCODER_2_IDX, "E2 Tempo 2"));
    
    // Add delay 1 tempo mode toggle (Encoder 1 button)
    AddParameter(new ToggleParameter("Tempo Mode", false, ENCODER_1_BUTTON_IDX, "On", "Off", -1));  // Hidden - tempo mode toggle
    
    // Add delay 2 tempo mode toggle (Encoder 2 button)
    AddParameter(new ToggleParameter("Tempo Mode", false, ENCODER_2_BUTTON_IDX, "On", "Off", -1));  // Hidden - tempo mode toggle
    
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
        float mix1 = parameters_[0]->GetValue();
        float feedback1 = parameters_[1]->GetValue();
        float subdivision1 = parameters_[2]->GetValue();
        float mix2 = parameters_[3]->GetValue();
        float feedback2 = parameters_[4]->GetValue();
        float subdivision2 = parameters_[5]->GetValue();
        
        // Delay 1 time parameter (index 6) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam1 = static_cast<TimeParameter*>(parameters_[6]);
        float delayTime1 = timeParam1->GetValue();
        
        // Delay 2 time parameter (index 7) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam2 = static_cast<TimeParameter*>(parameters_[7]);
        float delayTime2 = timeParam2->GetValue();
        
        // Delay 1 TempoMode toggle (index 8)
        if (parameters_[8]->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam1 = static_cast<ToggleParameter*>(parameters_[8]);
            bool newTempoMode1 = toggleParam1->GetState();
            
            // Only update display if the mode actually changed
            if (newTempoMode1 != tempoMode1_) {
                tempoMode1_ = newTempoMode1;
                
                // Update TimeParameter display mode based on toggle
                if (tempoMode1_) {
                    timeParam1->SetDisplayMode(TimeDisplayMode::TEMPO_BPM);
                } else {
                    timeParam1->SetDisplayMode(TimeDisplayMode::TIME_MS);
                }
                
                // Request display update since the parameter name changed
                RequestParameterDisplayUpdate(6); // Index 6 is delay 1 time parameter
                
                // Force update of delay 1 when tempo mode changes
                lastDelayTime1_ = -1.0f; // Force recalculation
            }
        }
        
        // Delay 2 TempoMode toggle (index 9)
        if (parameters_[9]->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam2 = static_cast<ToggleParameter*>(parameters_[9]);
            bool newTempoMode2 = toggleParam2->GetState();
            
            // Only update display if the mode actually changed
            if (newTempoMode2 != tempoMode2_) {
                tempoMode2_ = newTempoMode2;
                
                // Update TimeParameter display mode based on toggle
                if (tempoMode2_) {
                    timeParam2->SetDisplayMode(TimeDisplayMode::TEMPO_BPM);
                } else {
                    timeParam2->SetDisplayMode(TimeDisplayMode::TIME_MS);
                }
                
                // Request display update since the parameter name changed
                RequestParameterDisplayUpdate(7); // Index 7 is delay 2 time parameter
                
                // Force update of delay 2 when tempo mode changes
                lastDelayTime2_ = -1.0f; // Force recalculation
            }
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
