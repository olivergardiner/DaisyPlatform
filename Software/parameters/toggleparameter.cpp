#include "toggleparameter.h"
#include <cstdio>
#include <cstring>

using namespace perspective;

ToggleParameter::ToggleParameter(const char* name, bool defaultValue, int index, const char* trueStr, const char* falseStr, int displayIndex)
    : EffectParameter(name, 0.0f, 1.0f, defaultValue ? 1.0f : 0.0f, index, displayIndex)
    , state_(defaultValue)
{
    strncpy(trueStr_, trueStr, sizeof(trueStr_) - 1);
    trueStr_[sizeof(trueStr_) - 1] = '\0';
    strncpy(falseStr_, falseStr, sizeof(falseStr_) - 1);
    falseStr_[sizeof(falseStr_) - 1] = '\0';
}

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

void ToggleParameter::SetValue(float value) {
    // Any value >= 0.5 is considered ON, otherwise OFF.
    SetState(value >= 0.5f);
}

ParameterType ToggleParameter::GetType() const {
    return ParameterType::TOGGLE;
}

void ToggleParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer && bufferSize > 0) {
        snprintf(buffer, bufferSize, "%s", state_ ? trueStr_ : falseStr_);
    }
}
