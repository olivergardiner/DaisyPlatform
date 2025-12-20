#include "effectparameter.h"
#include <algorithm>
#include <cmath>

using namespace daisy;
using namespace perspective;

// ========== EffectParameter Base Class ==========

EffectParameter::EffectParameter(const std::string& name, float minValue, float maxValue, float defaultValue, int index)
    : name_(name)
    , minValue_(minValue)
    , maxValue_(maxValue)
    , currentValue_(defaultValue)
    , index_(index)
{}

EffectParameter::~EffectParameter() {}

float EffectParameter::GetValue() const {
    return currentValue_;
}

float EffectParameter::GetNormalizedValue() const {
    return (currentValue_ - minValue_) / (maxValue_ - minValue_);
}

const std::string& EffectParameter::GetName() const {
    return name_;
}

float EffectParameter::GetMin() const {
    return minValue_;
}

float EffectParameter::GetMax() const {
    return maxValue_;
}

int EffectParameter::GetIndex() const {
    return index_;
}

void EffectParameter::SetValue(float value) {
    currentValue_ = std::clamp(value, minValue_, maxValue_);
}

void EffectParameter::SetNormalizedValue(float normalizedValue) {
    float clampedNormalized = std::clamp(normalizedValue, 0.0f, 1.0f);
    currentValue_ = minValue_ + (clampedNormalized * (maxValue_ - minValue_));
}

void EffectParameter::SetIndex(int index) {
    index_ = index;
}

// ========== PotentiometerParameter ==========

PotentiometerParameter::PotentiometerParameter(const std::string& name, float minValue, float maxValue, float defaultValue, PotCurve curve, int index)
    : EffectParameter(name, minValue, maxValue, defaultValue, index)
    , curve_(curve)
{}

PotentiometerParameter::~PotentiometerParameter() {}

void PotentiometerParameter::SetCurve(PotCurve curve) {
    curve_ = curve;
}

PotCurve PotentiometerParameter::GetCurve() const {
    return curve_;
}

void PotentiometerParameter::SetNormalizedValueWithCurve(float normalizedValue) {
    float curvedValue = ApplyCurve(normalizedValue);
    currentValue_ = minValue_ + (curvedValue * (maxValue_ - minValue_));
}

ParameterType PotentiometerParameter::GetType() const {
    return ParameterType::POTENTIOMETER;
}

int PotentiometerParameter::GetValueAsInt(int maxInt) const {
    // Get normalized value (0.0 to 1.0)
    float normalized = GetNormalizedValue();
    
    // Scale to integer range (0 to maxInt)
    // Add 0.5 for proper rounding
    int intValue = static_cast<int>(normalized * maxInt + 0.5f);
    
    // Clamp to ensure we stay within bounds
    return std::clamp(intValue, 0, maxInt);
}

float PotentiometerParameter::ApplyCurve(float normalizedValue) {
    switch (curve_) {
        case PotCurve::LIN:
            return normalizedValue;
        
        case PotCurve::LOG:
            return taperFunction(normalizedValue, 0.12);
        
        case PotCurve::REVERSE_LOG:
            return taperFunction(normalizedValue, 0.88);
        
        case PotCurve::LOG_A:
            return taperFunction(normalizedValue, 0.25);
       
        case PotCurve::W_TAPER:
            // W taper - dual curve for blend/crossfade controls
            // Creates a smooth transition with equal power curve
            if (normalizedValue < 0.5f) {
                return 2.0f * normalizedValue * normalizedValue;
            } else {
                float inverse = 1.0f - normalizedValue;
                return 1.0f - (2.0f * inverse * inverse);
            }
        
        case PotCurve::SQUARED:
            // Squared curve (gentle exponential)
            return normalizedValue * normalizedValue;
        
        case PotCurve::CUBED:
            // Cubed curve (more aggressive exponential)
            return normalizedValue * normalizedValue * normalizedValue;
        
        default:
            return normalizedValue;
    }
}

// ========== EncoderParameter ==========

EncoderParameter::EncoderParameter(const std::string& name, float minValue, float maxValue, float defaultValue, float stepSize, int index)
    : EffectParameter(name, minValue, maxValue, defaultValue, index)
    , stepSize_(stepSize)
{}

EncoderParameter::~EncoderParameter() {}

void EncoderParameter::SetStepSize(float stepSize) {
    stepSize_ = stepSize;
}

float EncoderParameter::GetStepSize() const {
    return stepSize_;
}

void EncoderParameter::Increment(int steps) {
    currentValue_ += steps * stepSize_;
    currentValue_ = std::clamp(currentValue_, minValue_, maxValue_);
}

void EncoderParameter::Decrement(int steps) {
    currentValue_ -= steps * stepSize_;
    currentValue_ = std::clamp(currentValue_, minValue_, maxValue_);
}

ParameterType EncoderParameter::GetType() const {
    return ParameterType::ENCODER;
}

// ========== ToggleParameter ==========

ToggleParameter::ToggleParameter(const std::string& name, bool defaultValue, int index)
    : EffectParameter(name, 0.0f, 1.0f, defaultValue ? 1.0f : 0.0f, index)
    , state_(defaultValue)
{}

ToggleParameter::~ToggleParameter() {}

void ToggleParameter::Toggle() {
    state_ = !state_;
    currentValue_ = state_ ? 1.0f : 0.0f;
}

bool ToggleParameter::GetState() const {
    return state_;
}

void ToggleParameter::SetState(bool state) {
    state_ = state;
    currentValue_ = state_ ? 1.0f : 0.0f;
}

ParameterType ToggleParameter::GetType() const {
    return ParameterType::TOGGLE;
}
