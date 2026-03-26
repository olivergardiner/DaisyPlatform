#include "delayeffect.h"
#include "../controls.h"
#include "../parameters/timeparameter.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

// Epsilon for delay time comparison (0.5 samples is imperceptible)
static constexpr float DELAY_EPSILON = 0.5f;

// Static pool of delay lines in SDRAM (supports up to MAX_DELAY_INSTANCES)
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayPoolL_[MAX_DELAY_INSTANCES];
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayPoolR_[MAX_DELAY_INSTANCES];
static bool delayPoolUsed_[MAX_DELAY_INSTANCES] = {false};

// Allocate a delay line pair from the pool
static int AllocateDelayInstance() {
    for (size_t i = 0; i < MAX_DELAY_INSTANCES; i++) {
        if (!delayPoolUsed_[i]) {
            delayPoolUsed_[i] = true;
            return static_cast<int>(i);
        }
    }
    return -1; // Pool exhausted
}

// Free a delay line pair back to the pool
static void FreeDelayInstance(int id) {
    if (id >= 0 && static_cast<size_t>(id) < MAX_DELAY_INSTANCES) {
        delayPoolUsed_[id] = false;
    }
}


DelayEffect::DelayEffect() 
    : TempoEffect("Delay")
    , delayL_(nullptr)
    , delayR_(nullptr)
    , instanceId_(-1)
    , delayBuffersAllocated_(false) {
}

DelayEffect::~DelayEffect() {
    ReleaseDelayLines();
}

bool DelayEffect::TryAllocateDelayLines() {
    if (delayBuffersAllocated_) {
        return true;
    }

    instanceId_ = AllocateDelayInstance();
    if (instanceId_ < 0) {
        return false;
    }

    delayL_ = &delayPoolL_[instanceId_];
    delayR_ = &delayPoolR_[instanceId_];
    delayL_->Init();
    delayR_->Init();
    delayBuffersAllocated_ = true;
    return true;
}

void DelayEffect::ReleaseDelayLines() {
    if (instanceId_ >= 0) {
        FreeDelayInstance(instanceId_);
        instanceId_ = -1;
    }
    delayBuffersAllocated_ = false;
    delayL_ = nullptr;
    delayR_ = nullptr;
}

void DelayEffect::OnSelected() {
    // Buffers are allocated once during Init(); keep parameter-derived state fresh.
    Update();
}

void DelayEffect::OnDeselected() {
    // Keep buffers owned for the lifetime of this effect instance.
}

void DelayEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // Claim delay buffers once at startup (before audio thread starts).
    TryAllocateDelayLines();
    
    // Initialize metronome bass drum
    metronomeDrum_.Init(sampleRate);
    metronomeDrum_.SetFreq(60.0f);  // Low bass frequency
    metronomeDrum_.SetTone(0.5f);
    metronomeDrum_.SetDecay(0.3f);
    metronomeDrum_.SetAccent(0.8f);
    samplesUntilNextBeat_ = 0.0f;
    
    // Add parameters: Mix, Feedback, Subdivision, Time, TempoToggle
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display mix as percentage
    
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display feedback as percentage
    
    AddParameter(new PotentiometerParameter("K3 Subdivision", 0.0f, 7.0f, 3.0f, PotCurve::LIN, KNOB_3_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kSubdivisionGlyphs, 8);
    subdivisionParamIndex_ = 2;  // Track subdivision parameter index
    
    // TimeParameter with milliseconds range (10-2000 ms), 1ms step in time mode, 0.5 BPM in tempo mode
    AddParameter(new TimeParameter("E1 Time", 10.0f, 2000.0f, 500.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo"));
    timeParamIndex_ = 3;  // Track time parameter index
    
    AddParameter(new ToggleParameter("Tempo Mode", false, ENCODER_1_BUTTON_IDX, "On", "Off", -1));  // Hidden - tempo mode toggle
    
    // Set default delay parameters
    Update();
}

void DelayEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_ || !delayBuffersAllocated_ || !delayL_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[kParamFeedback]->GetValue() : 0.5f;
    
    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float delayed = delayL_->Read();
        delayL_->Write(in[i] + delayed * feedback);
        
        out[i] = wetOnly_ ? (delayed * mix) : (in[i] + delayed * mix);
    }
}

void DelayEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_ || !delayBuffersAllocated_ || !delayL_ || !delayR_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[kParamFeedback]->GetValue() : 0.5f;
    
    // Process stereo signal with independent delays
    for (size_t i = 0; i < size; i++) {
        float delayedL = delayL_->Read();
        float delayedR = delayR_->Read();
        
        delayL_->Write(inL[i] + delayedL * feedback);
        delayR_->Write(inR[i] + delayedR * feedback);
        
        // Mix in metronome
        float metronome = ProcessMetronome();
        
        outL[i] = wetOnly_ ? (delayedL * mix) : (inL[i] + delayedL * mix);
        outR[i] = wetOnly_ ? (delayedR * mix) : (inR[i] + delayedR * mix);
        
        // Add metronome to both channels
        outL[i] += metronome;
        outR[i] += metronome;
    }
}

void DelayEffect::Update() {
    // Update delay parameters from effect parameters
    if (parameters_.size() >= 5) {
        // Mix parameter (index 0) - handled in Process
        // Feedback parameter (index 1) - handled in Process
        // Subdivision parameter (index 2) - handled in CalculateDelayTimeFromTempo
        
        // Time parameter (index 3) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam = static_cast<TimeParameter*>(parameters_[kParamTime]);
        baseDelayTime_ = timeParam->GetValueAsMs() / 1000.0f; // Convert ms to seconds
        
        // TempoMode toggle (index 4)
        if (parameters_[kParamTempoMode]->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam = static_cast<ToggleParameter*>(parameters_[kParamTempoMode]);
            bool newTempoMode = toggleParam->GetState();
            
            // Only update display if the mode actually changed
            if (newTempoMode != tempoMode_) {
                tempoMode_ = newTempoMode;
                
                // Update TimeParameter display mode based on toggle
                if (tempoMode_) {
                    timeParam->SetDisplayMode(TimeDisplayMode::TEMPO_BPM);
                } else {
                    timeParam->SetDisplayMode(TimeDisplayMode::TIME_MS);
                }
                
                // Request display update since the parameter name changed
                RequestParameterDisplayUpdate(3); // Index 3 is the time parameter
            }
        }
        
        // Note: metronomeEnabled_ is now set globally by Perspective via SetMetronomeEnabled()
        
        // Calculate effective delay time based on mode
        // Always apply subdivision multiplier, whether in tempo or time mode
        effectiveDelayTime_ = tempoMode_ ? CalculateDelayTimeFromTempo() : (baseDelayTime_ * GetSubdivisionMultiplier());
        
        // Update delay line only when parameters change (not on every audio callback)
        float delaySamples = sampleRate_ * effectiveDelayTime_;
        if (std::abs(delaySamples - currentDelaySamples_) > DELAY_EPSILON) {
            currentDelaySamples_ = delaySamples;
            if (delayBuffersAllocated_ && delayL_ && delayR_) {
                delayL_->SetDelay(delaySamples);
                delayR_->SetDelay(delaySamples);
            }
        }
    }
}
