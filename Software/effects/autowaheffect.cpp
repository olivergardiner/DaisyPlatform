#include "autowaheffect.h"
#include "../controls.h"
#include "../parameters/valueparameter.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

static constexpr float MIN_DETECTOR_GAIN = 5.0f;
static constexpr float MAX_DETECTOR_GAIN = 9.0f;
static constexpr float ATTACK_MIN_SEC = 0.001f;
static constexpr float ATTACK_MAX_SEC = 0.5f;

static float DetectorGainFromAttack(float attackTimeSec) {
    float clampedAttack = std::max(ATTACK_MIN_SEC, std::min(attackTimeSec, ATTACK_MAX_SEC));
    float normalized = (clampedAttack - ATTACK_MIN_SEC) / (ATTACK_MAX_SEC - ATTACK_MIN_SEC);
    return MIN_DETECTOR_GAIN + normalized * (MAX_DETECTOR_GAIN - MIN_DETECTOR_GAIN);
}

AutowahEffect::AutowahEffect()
    : Effect("Autowah")
    , envelope_(0.0f)
    , detectorInput_(0.0f) {
}

AutowahEffect::~AutowahEffect() {
}

void AutowahEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize state variable filters for stereo
    filterL_.Init(sampleRate);
    filterR_.Init(sampleRate);
    
    // Add parameters: Mix, Resonance, Frequency, Attack, Release, Sensitivity (signed Hz offset)
    auto* mixParam = new ValueParameter("K1 Mix", 0.0f, 1.0f, 0.5f);
    mixParam->BindPotentiometer(MACRO_KNOB_MIX_IDX, PotCurve::LIN);
    mixParam->SetMacroRole(MacroRole::MIX);
    AddParameter(mixParam);

    AddParameter(new ValueParameter("Resonance", 0.0f, 1.0f, 0.85f));
    AddParameter(new ValueParameter("Frequency", 400.0f, 2000.0f, 1000.0f));

    auto* attackParam = new ValueParameter("Attack ms", 0.001f, 1.0f, 0.2f);
    attackParam->SetDisplayType(DisplayType::SCALED);
    attackParam->SetScaleFactor(1000.0f);
    AddParameter(attackParam);

    auto* releaseParam = new ValueParameter("Release ms", 0.001f, 0.5f, 0.01f);
    releaseParam->SetDisplayType(DisplayType::SCALED);
    releaseParam->SetScaleFactor(1000.0f);
    AddParameter(releaseParam);

    auto* sensitivityParam = new ValueParameter("K6 Sensitivity", -3000.0f, 3000.0f, 1600.0f);
    sensitivityParam->BindPotentiometer(KNOB_6_IDX, PotCurve::LIN);
    AddParameter(sensitivityParam);
    
    // Set default filter parameters
    Update();
}

void AutowahEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            out[i] = in[i];
        }
        return;
    }
    
    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;

    // Get parameters
    float baseFreq = parameters_.size() >= 6 ? parameters_[kParamFrequency]->GetValue() : 1000.0f;
    float attackTime = parameters_.size() >= 6 ? parameters_[kParamAttackMs]->GetValue() : 0.1f;
    float releaseTime = parameters_.size() >= 6 ? parameters_[kParamReleaseMs]->GetValue() : 0.001f;
    float sensitivityHz = parameters_.size() >= 6 ? parameters_[kParamSensitivity]->GetValue() : 1600.0f;

    // Convert attack/release times to one-pole coefficients.
    float attackSec = std::max(0.0001f, attackTime);
    float releaseSec = std::max(0.0001f, releaseTime);
    float detectorGain = DetectorGainFromAttack(attackSec);
    float attackAlpha = 1.0f - expf(-1.0f / (sampleRate_ * attackSec));
    float releaseAlpha = 1.0f - expf(-1.0f / (sampleRate_ * releaseSec));

    // Process with wet/dry blend
    for (size_t i = 0; i < size; i++) {
        float detectorInput = std::min(std::fabs(in[i]) * detectorGain, 1.0f);
        detectorInput_ = detectorInput;

        // Envelope follower with independent attack/release time constants.
        if (detectorInput > envelope_) {
            envelope_ += attackAlpha * (detectorInput - envelope_);
        } else {
            envelope_ += releaseAlpha * (detectorInput - envelope_);
        }

        // Apply detector gain to make envelope response more usable with guitar-level signals.
        float scaledEnvelope = std::min(envelope_ * detectorGain, 1.0f);

        // Sensitivity sets the maximum frequency deviation from base frequency in Hz.
        float modulatedFreq = baseFreq + (scaledEnvelope * sensitivityHz);
        float floorHz = (sensitivityHz >= 0.0f) ? 20.0f : 100.0f;
        modulatedFreq = std::max(floorHz, std::min(modulatedFreq, 5000.0f));
        filterL_.SetFreq(modulatedFreq);

        filterL_.Process(in[i]);
        float wet = filterL_.Band();  // Get bandpass output
        out[i] = in[i] * (1.0f - mix) + wet * mix;
    }
}

void AutowahEffect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        // Bypass - pass through dry signal
        for (size_t i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    
    // Get mix parameter
    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    
    // Get parameters
    float baseFreq = parameters_.size() >= 6 ? parameters_[kParamFrequency]->GetValue() : 1000.0f;
    float attackTime = parameters_.size() >= 6 ? parameters_[kParamAttackMs]->GetValue() : 0.1f;
    float releaseTime = parameters_.size() >= 6 ? parameters_[kParamReleaseMs]->GetValue() : 0.001f;
    float sensitivityHz = parameters_.size() >= 6 ? parameters_[kParamSensitivity]->GetValue() : 1600.0f;

    float attackSec = std::max(0.0001f, attackTime);
    float releaseSec = std::max(0.0001f, releaseTime);
    float detectorGain = DetectorGainFromAttack(attackSec);
    float attackAlpha = 1.0f - expf(-1.0f / (sampleRate_ * attackSec));
    float releaseAlpha = 1.0f - expf(-1.0f / (sampleRate_ * releaseSec));

    // Process stereo signal with independent filters
    for (size_t i = 0; i < size; i++) {
        // Compute average envelope of both inputs
        float avgInput = (std::abs(inL[i]) + std::abs(inR[i])) * 0.5f;
        float detectorInput = std::min(avgInput * detectorGain, 1.0f);
        detectorInput_ = detectorInput;
        
        // Envelope follower with independent attack/release time constants.
        if (detectorInput > envelope_) {
            envelope_ += attackAlpha * (detectorInput - envelope_);
        } else {
            envelope_ += releaseAlpha * (detectorInput - envelope_);
        }
        
        // Apply detector gain to make envelope response more usable with guitar-level signals.
        float scaledEnvelope = std::min(envelope_ * detectorGain, 1.0f);

        // Sensitivity sets the maximum frequency deviation from base frequency in Hz.
        float modulatedFreq = baseFreq + (scaledEnvelope * sensitivityHz);
        float floorHz = (sensitivityHz >= 0.0f) ? 20.0f : 100.0f;
        modulatedFreq = std::max(floorHz, std::min(modulatedFreq, 5000.0f));
        filterL_.SetFreq(modulatedFreq);
        filterR_.SetFreq(modulatedFreq);
        
        filterL_.Process(inL[i]);
        filterR_.Process(inR[i]);
        
        float wetL = filterL_.Band();  // Get bandpass output
        float wetR = filterR_.Band();
        
        outL[i] = inL[i] * (1.0f - mix) + wetL * mix;
        outR[i] = inR[i] * (1.0f - mix) + wetR * mix;
    }
}

void AutowahEffect::Update() {
    // Update filter parameters from effect parameters
    if (parameters_.size() >= 6) {
        // Mix parameter (index 0) - handled in Process
        // Frequency parameter (index 2) - handled in Process
        // Attack parameter (index 3) - handled in Process
        // Release parameter (index 4) - handled in Process
        // Sensitivity parameter (index 5) - handled in Process
        
        // Resonance parameter (index 1) maps directly to the SVF resonance input
        float resonance = parameters_[kParamResonance]->GetValue();
        filterL_.SetRes(resonance);
        filterR_.SetRes(resonance);
    }
}

float AutowahEffect::GetEnvelopeBrightness() const {
    return std::min(detectorInput_, 1.0f);
}
