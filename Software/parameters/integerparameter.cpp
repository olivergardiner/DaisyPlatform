#include "integerparameter.h"
#include <cstdio>
#include <cmath>

using namespace perspective;

IntegerParameter::IntegerParameter(const char* name, int minValue, int maxValue, int defaultValue, int displayIndex)
    : EffectParameter(name, static_cast<float>(minValue), static_cast<float>(maxValue), static_cast<float>(defaultValue), displayIndex)
{}

IntegerParameter::~IntegerParameter() {}

ParameterKind IntegerParameter::GetKind() const {
    return ParameterKind::INTEGER;
}

int IntegerParameter::GetIntValue() const {
    return static_cast<int>(std::lround(GetValue()));
}

void IntegerParameter::SetIntValue(int value) {
    SetValue(static_cast<float>(value));
}

void IntegerParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }
    snprintf(buffer, bufferSize, "%d", GetIntValue());
}

void IntegerParameter::SetNormalizedValueWithCurve(float normalizedValue) {
    float curved = ApplyPotCurve(curve_, clamp(normalizedValue, 0.0f, 1.0f));
    float value = minValue_ + curved * (maxValue_ - minValue_);
    currentValue_ = clamp(std::round(value), minValue_, maxValue_);
}

void IntegerParameter::Increment(int steps) {
    float next = std::round(GetValue()) + static_cast<float>(reversed_ ? -steps : steps);
    currentValue_ = clamp(next, minValue_, maxValue_);
}

void IntegerParameter::Decrement(int steps) {
    float next = std::round(GetValue()) + static_cast<float>(reversed_ ? steps : -steps);
    currentValue_ = clamp(next, minValue_, maxValue_);
}
