#include "potentiometerparameter.h"
#include <cmath>
#include <cstdio>

using namespace perspective;

PotentiometerParameter::PotentiometerParameter(const char* name, float minValue, float maxValue, float defaultValue, PotCurve curve, int index, int displayIndex)
    : EffectParameter(name, minValue, maxValue, defaultValue, index, displayIndex)
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
