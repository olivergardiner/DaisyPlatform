#include "moddelayeffect.h"
#include "../controls.h"
#include "../parameters/encoderparameter.h"
#include "../parameters/timeparameter.h"
#include "../parameters/toggleparameter.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

// Epsilon for delay time comparison (0.5 samples is imperceptible)
static constexpr float DELAY_EPSILON = 0.5f;

constexpr size_t MAX_DELAY = 48000 * 4; // 4 seconds max delay at 48kHz
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayL_;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayR_;


static const char *lfoWaveShapes[8] = {
    "Sin", "Tri", "Saw", "Ramp", "Square", "PB Tri", "PB Saw", "PB Sq"
};

ModDelayEffect::ModDelayEffect() 
    : TempoEffect("Mod Delay") {
}

ModDelayEffect::~ModDelayEffect() {
}

void ModDelayEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize delay lines for stereo
    delayL_.Init();
    delayR_.Init();
    
    // Initialize metronome bass drum
    metronomeDrum_.Init(sampleRate);
    metronomeDrum_.SetFreq(60.0f);  // Low bass frequency
    metronomeDrum_.SetTone(0.5f);
    metronomeDrum_.SetDecay(0.3f);
    metronomeDrum_.SetAccent(1.0f);
    samplesUntilNextBeat_ = 0.0f;
    
    // Initialize modulation LFOs
    lfoL_.Init(sampleRate);
    lfoL_.SetWaveform(Oscillator::WAVE_SIN);
    lfoL_.SetAmp(1.0f);
    lfoL_.SetFreq(0.5f);
    
    lfoR_.Init(sampleRate);
    lfoR_.SetWaveform(Oscillator::WAVE_SIN);
    lfoR_.SetAmp(1.0f);
    lfoR_.SetFreq(0.5f);
    
    // Add parameters: Mix, Feedback, ModRate, ModDepth, Subdivision, Time/Tempo, Wave Shape, TempoToggle
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.50f, PotCurve::LIN, KNOB_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display mix as percentage
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display feedback as percentage
    AddParameter(new PotentiometerParameter("K3 Mod Rate", 0.0f, 10.0f, 0.5f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("K4 Mod Depth", 0.0f, 1.0f, 0.0f, PotCurve::LIN, KNOB_4_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f); // Display depth as percentage
    AddParameter(new PotentiometerParameter("K5 Subdivision", 0.0f, 7.0f, 3.0f, PotCurve::LIN, KNOB_5_IDX)); // 8 subdivisions: 1-8 sixteenths, default to quarter note (4 sixteenths)
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kSubdivisionGlyphs, 8);
    subdivisionParamIndex_ = 4;  // Track subdivision parameter index
    char valueStr[16];
    parameters_.back()->GetValueAsString(valueStr, sizeof(valueStr));
    
    // TimeParameter with milliseconds range (10-2000 ms), displayed as Time or Tempo
    AddParameter(new TimeParameter("E1 Time", 10.0f, 2000.0f, 500.0f, 1.0f, ENCODER_1_IDX, "E1 Tempo"));
    timeParamIndex_ = 5;  // Track time parameter index

    AddParameter(new EncoderParameter("E2 Wave", 0.0f, static_cast<float>(Oscillator::WAVE_LAST - 1), 0.0f, 1.0f, ENCODER_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(lfoWaveShapes, 8);
    
    AddParameter(new ToggleParameter("Tempo Mode", false, ENCODER_1_BUTTON_IDX, "On", "Off",-1)); // Encoder 1 switch
    
    // Note: Metronome is now controlled globally by Perspective via SetMetronomeEnabled()
    
    // Set default delay parameters
    Update();
}

void ModDelayEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[kParamFeedback]->GetValue() : 0.5f;
    float modDepth = parameters_.size() > 3 ? parameters_[kParamModDepth]->GetValue() : 0.0f;
    
    // Use cached effective delay time (updated in Update())
    float effectiveDelayTime = effectiveDelayTime_;
    
    // Optimization: only modulate delay time when modDepth > 0
    static constexpr float MOD_THRESHOLD = 0.001f; // 0.1% threshold
    bool hasModulation = modDepth > MOD_THRESHOLD;
    
    // Process with wet/dry blend and modulation
    for (size_t i = 0; i < size; i++) {
        if (hasModulation) {
            // Apply modulation to delay time
            float lfoValue = lfoL_.Process();
            float modulatedTime = effectiveDelayTime + (lfoValue * modDepth * 0.001f); // modDepth in ms
            modulatedTime = fclamp(modulatedTime, 0.001f, 2.0f);
            float delaySamples = sampleRate_ * modulatedTime;
            delayL_.SetDelay(delaySamples);
        } else {
            // Skip LFO processing when not modulating
            lfoL_.Process();
        }
        
        float delayed = delayL_.Read();
        delayL_.Write(in[i] + delayed * feedback);
        
        out[i] = wetOnly_ ? (delayed * mix) : (in[i] + delayed * mix);
    }
}

void ModDelayEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
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
    float modDepth = parameters_.size() > 3 ? parameters_[kParamModDepth]->GetValue() : 0.0f;
    
    // Use cached effective delay time (updated in Update())
    float effectiveDelayTime = effectiveDelayTime_;
    
    // Optimization: only modulate delay time when modDepth > 0
    static constexpr float MOD_THRESHOLD = 0.001f; // 0.1% threshold
    bool hasModulation = modDepth > MOD_THRESHOLD;
    
    // Process stereo signal with independent delays and modulation
    for (size_t i = 0; i < size; i++) {
        if (hasModulation) {
            // Apply modulation to delay times (stereo LFOs for wider effect)
            float lfoValueL = lfoL_.Process();
            float lfoValueR = lfoR_.Process();
            
            float modulatedTimeL = effectiveDelayTime + (lfoValueL * modDepth * 0.001f); // modDepth in ms
            float modulatedTimeR = effectiveDelayTime + (lfoValueR * modDepth * 0.001f);
            modulatedTimeL = fclamp(modulatedTimeL, 0.001f, 2.0f);
            modulatedTimeR = fclamp(modulatedTimeR, 0.001f, 2.0f);
            
            delayL_.SetDelay(sampleRate_ * modulatedTimeL);
            delayR_.SetDelay(sampleRate_ * modulatedTimeR);
        } else {
            // Skip LFO processing when not modulating
            lfoL_.Process();
            lfoR_.Process();
        }
        
        float delayedL = delayL_.Read();
        float delayedR = delayR_.Read();
        
        delayL_.Write(inL[i] + delayedL * feedback);
        delayR_.Write(inR[i] + delayedR * feedback);
        
        // Mix in metronome
        float metronome = ProcessMetronome();
        
        outL[i] = wetOnly_ ? (delayedL * mix) : (inL[i] + delayedL * mix);
        outR[i] = wetOnly_ ? (delayedR * mix) : (inR[i] + delayedR * mix);
        
        // Add metronome to both channels
        outL[i] += metronome;
        outR[i] += metronome;
    }
}

void ModDelayEffect::Update() {
    // Update delay parameters from effect parameters
    if (parameters_.size() >= 8) {
        // Mix parameter (index 0) - handled in Process
        // Feedback parameter (index 1) - handled in Process
        
        // ModRate parameter (index 2)
        float modRate = parameters_[kParamModRate]->GetValue();
        lfoL_.SetFreq(modRate);
        // Offset right channel LFO by 90 degrees for stereo width
        lfoR_.SetFreq(modRate);
        lfoR_.PhaseAdd(0.25f); // 90 degree offset
        
        // ModDepth parameter (index 3) - handled in Process
        // Subdivision parameter (index 4) - handled in CalculateDelayTimeFromTempo
        
        // Time parameter (index 5) - TimeParameter stores value in milliseconds
        TimeParameter* timeParam = static_cast<TimeParameter*>(parameters_[kParamTime]);
        baseDelayTime_ = timeParam->GetValueAsMs() / 1000.0f; // Convert ms to seconds

        // Wave shape parameter (index 6) - controlled by Encoder 2
        int waveform = static_cast<int>(parameters_[kParamWaveShape]->GetValue());
        waveform = clamp(waveform, 0, static_cast<int>(Oscillator::WAVE_LAST - 1));
        lfoL_.SetWaveform(static_cast<uint8_t>(waveform));
        lfoR_.SetWaveform(static_cast<uint8_t>(waveform));
        
        // TempoMode toggle (index 7)
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
                RequestParameterDisplayUpdate(5); // Index 5 is the time parameter
            }
        }
        
        // Calculate effective delay time based on mode
        // Always apply subdivision multiplier, whether in tempo or time mode
        effectiveDelayTime_ = tempoMode_ ? CalculateDelayTimeFromTempo() : (baseDelayTime_ * GetSubdivisionMultiplier());
        
        // Update delay line only when parameters change (not on every audio callback)
        float delaySamples = sampleRate_ * effectiveDelayTime_;
        if (std::abs(delaySamples - currentDelaySamples_) > DELAY_EPSILON) {
            currentDelaySamples_ = delaySamples;
            delayL_.SetDelay(delaySamples);
            delayR_.SetDelay(delaySamples);
        }
    }
    
    // Note: metronomeEnabled_ is now set globally by Perspective via SetMetronomeEnabled()
}
