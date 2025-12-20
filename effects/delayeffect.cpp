#include "delayeffect.h"
#include "../controls.h"

using namespace perspective;
using namespace daisysp;

DelayEffect::DelayEffect() 
    : Effect("Delay")
    , baseDelayTime_(0.5f)
    , effectiveDelayTime_(0.5f)
    , tempoMode_(false) {
}

DelayEffect::~DelayEffect() {
}

void DelayEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize delay lines for stereo
    delayL_.Init();
    delayR_.Init();
    
    // Initialize modulation LFOs
    lfoL_.Init(sampleRate);
    lfoL_.SetWaveform(Oscillator::WAVE_SIN);
    lfoL_.SetAmp(1.0f);
    lfoL_.SetFreq(0.5f);
    
    lfoR_.Init(sampleRate);
    lfoR_.SetWaveform(Oscillator::WAVE_SIN);
    lfoR_.SetAmp(1.0f);
    lfoR_.SetFreq(0.5f);
    
    // Add parameters: Mix, Feedback, ModRate, ModDepth, Subdivision, Time/Tempo, TempoToggle
    AddParameter(new PotentiometerParameter("Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("Feedback", 0.0f, 0.95f, 0.5f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("ModRate", 0.0f, 10.0f, 0.5f, PotCurve::LOG, KNOB_3_IDX));
    AddParameter(new PotentiometerParameter("ModDepth", 0.0f, 50.0f, 0.0f, PotCurve::LIN, KNOB_4_IDX));
    AddParameter(new PotentiometerParameter("Subdivision", 0.0f, 6.0f, 3.0f, PotCurve::LIN, KNOB_5_IDX)); // 7 subdivisions: 1-6 and 8 sixteenths, default to quarter note (4 sixteenths)
    AddParameter(new EncoderParameter("Time", 0.001f, 2.0f, 0.5f, 0.005f, ENCODER_1_IDX));
    AddParameter(new ToggleParameter("TempoMode", false, ENCODER_2_BUTTON_IDX)); // Encoder 2 switch
    
    // Set default delay parameters
    Update();
}

void DelayEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[1]->GetValue() : 0.5f;
    float modDepth = parameters_.size() > 3 ? parameters_[3]->GetValue() : 0.0f;
    
    // Calculate effective delay time based on mode
    float effectiveDelayTime = tempoMode_ ? CalculateDelayTimeFromTempo() : baseDelayTime_;
    
    // Process with wet/dry blend and modulation
    for (size_t i = 0; i < size; i++) {
        // Apply modulation to delay time
        float lfoValue = lfoL_.Process();
        float modulatedTime = effectiveDelayTime + (lfoValue * modDepth * 0.001f); // modDepth in ms
        modulatedTime = fclamp(modulatedTime, 0.001f, 2.0f);
        float delaySamples = sampleRate_ * modulatedTime;
        delayL_.SetDelay(delaySamples);
        
        float delayed = delayL_.Read();
        delayL_.Write(in[i] + delayed * feedback);
        
        out[i] = in[i] * (1.0f - mix) + delayed * mix;
    }
}

void DelayEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get parameters
    float mix = parameters_.size() > 0 ? parameters_[0]->GetValue() : 0.5f;
    float feedback = parameters_.size() > 1 ? parameters_[1]->GetValue() : 0.5f;
    float modDepth = parameters_.size() > 3 ? parameters_[3]->GetValue() : 0.0f;
    
    // Use cached effective delay time (updated in Update())
    float effectiveDelayTime = effectiveDelayTime_;
    
    // Process stereo signal with independent delays and modulation
    for (size_t i = 0; i < size; i++) {
        // Apply modulation to delay times (stereo LFOs for wider effect)
        float lfoValueL = lfoL_.Process();
        float lfoValueR = lfoR_.Process();
        
        float modulatedTimeL = effectiveDelayTime + (lfoValueL * modDepth * 0.001f); // modDepth in ms
        float modulatedTimeR = effectiveDelayTime + (lfoValueR * modDepth * 0.001f);
        modulatedTimeL = fclamp(modulatedTimeL, 0.001f, 2.0f);
        modulatedTimeR = fclamp(modulatedTimeR, 0.001f, 2.0f);
        
        delayL_.SetDelay(sampleRate_ * modulatedTimeL);
        delayR_.SetDelay(sampleRate_ * modulatedTimeR);
        
        float delayedL = delayL_.Read();
        float delayedR = delayR_.Read();
        
        delayL_.Write(inL[i] + delayedL * feedback);
        delayR_.Write(inR[i] + delayedR * feedback);
        
        outL[i] = inL[i] * (1.0f - mix) + delayedL * mix;
        outR[i] = inR[i] * (1.0f - mix) + delayedR * mix;
    }
}

void DelayEffect::Update() {
    // Update delay parameters from effect parameters
    if (parameters_.size() >= 7) {
        // Mix parameter (index 0) - handled in Process
        // Feedback parameter (index 1) - handled in Process
        
        // ModRate parameter (index 2)
        float modRate = parameters_[2]->GetValue();
        lfoL_.SetFreq(modRate);
        // Offset right channel LFO by 90 degrees for stereo width
        lfoR_.SetFreq(modRate);
        lfoR_.PhaseAdd(0.25f); // 90 degree offset
        
        // ModDepth parameter (index 3) - handled in Process
        // Subdivision parameter (index 4) - handled in CalculateDelayTimeFromTempo
        
        // Time parameter (index 5) - in seconds or BPM depending on mode
        baseDelayTime_ = parameters_[5]->GetValue();
        
        // TempoMode toggle (index 6)
        if (parameters_[6]->GetType() == ParameterType::TOGGLE) {
            ToggleParameter* toggleParam = static_cast<ToggleParameter*>(parameters_[6]);
            tempoMode_ = toggleParam->GetState();
        }
        
        // Calculate effective delay time based on mode
        effectiveDelayTime_ = tempoMode_ ? CalculateDelayTimeFromTempo() : baseDelayTime_;
    }
}

float DelayEffect::CalculateDelayTimeFromTempo() {
    // When in tempo mode, interpret the Time parameter as BPM (60-240 BPM)
    // Map 0.001-2.0 range to 60-240 BPM
    float bpm = 60.0f + (baseDelayTime_ / 2.0f) * 180.0f;
    bpm = fclamp(bpm, 60.0f, 240.0f);
    
    // Use tempo_ from Effect base class if available (from tap tempo)
    if (tempo_ > 0.0f) {
        bpm = tempo_ * 60.0f; // Convert Hz to BPM
    }
    
    // Calculate delay time for quarter note at this tempo
    // 60 seconds / BPM = seconds per beat (quarter note)
    float secondsPerBeat = 60.0f / bpm;
    
    // Apply subdivision multiplier
    float subdivisionMultiplier = GetSubdivisionMultiplier();
    
    return secondsPerBeat * subdivisionMultiplier;
}

float DelayEffect::GetSubdivisionMultiplier() const {
    if (parameters_.size() <= 4 || parameters_[4]->GetType() != ParameterType::POTENTIOMETER) {
        return SUBDIVISION_4_16TH; // Default to quarter note (4 sixteenths)
    }
    
    PotentiometerParameter* subdivParam = static_cast<PotentiometerParameter*>(parameters_[4]);
    int subdivIndex = subdivParam->GetValueAsInt(6); // 0-6 for 7 subdivisions
    
    // Map index to subdivision multiplier (in sixteenths)
    switch (subdivIndex) {
        case 0: return SUBDIVISION_1_16TH;  // 1 sixteenth
        case 1: return SUBDIVISION_2_16TH;  // 2 sixteenths (eighth)
        case 2: return SUBDIVISION_3_16TH;  // 3 sixteenths (dotted eighth)
        case 3: return SUBDIVISION_4_16TH;  // 4 sixteenths (quarter) - default
        case 4: return SUBDIVISION_5_16TH;  // 5 sixteenths
        case 5: return SUBDIVISION_6_16TH;  // 6 sixteenths (dotted quarter)
        case 6: return SUBDIVISION_8_16TH;  // 8 sixteenths (half)
        default: return SUBDIVISION_4_16TH;
    }
}
