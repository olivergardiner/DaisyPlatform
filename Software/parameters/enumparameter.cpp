#include "enumparameter.h"
#include <cstdio>

using namespace perspective;

namespace {
int ClampIndex(int index, int count) {
    if (count <= 0) return 0;
    return clamp(index, 0, count - 1);
}
} // namespace

EnumParameter::EnumParameter(const char* name, const char** options, int count, int defaultIndex, int displayIndex)
    : EffectParameter(name, 0.0f, count > 1 ? static_cast<float>(count - 1) : 0.0f,
                       static_cast<float>(ClampIndex(defaultIndex, count)), displayIndex)
    , options_(options)
    , optionCount_(count)
    , wrap_(true)
    , ownsOptions_(false)
{}

EnumParameter::EnumParameter(const char* name, std::initializer_list<const char*> options, int defaultIndex, int displayIndex)
    : EffectParameter(name, 0.0f, options.size() > 1 ? static_cast<float>(options.size() - 1) : 0.0f,
                       static_cast<float>(ClampIndex(defaultIndex, static_cast<int>(options.size()))), displayIndex)
    , options_(nullptr)
    , optionCount_(static_cast<int>(options.size()))
    , wrap_(true)
    , ownsOptions_(true)
{
    const char** copy = new const char*[optionCount_ > 0 ? optionCount_ : 1];
    int i = 0;
    for (const char* option : options) {
        copy[i++] = option;
    }
    options_ = copy;
}

EnumParameter::~EnumParameter() {
    if (ownsOptions_) {
        delete[] options_;
    }
}

ParameterKind EnumParameter::GetKind() const {
    return ParameterKind::ENUM;
}

int EnumParameter::GetSelectedIndex() const {
    return ClampIndex(static_cast<int>(GetValue() + 0.5f), optionCount_);
}

void EnumParameter::SetSelectedIndex(int index) {
    currentValue_ = static_cast<float>(ClampIndex(index, optionCount_));
}

int EnumParameter::GetOptionCount() const {
    return optionCount_;
}

void EnumParameter::SetWrap(bool wrap) {
    wrap_ = wrap;
}

bool EnumParameter::GetWrap() const {
    return wrap_;
}

void EnumParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }
    if (options_ != nullptr && optionCount_ > 0) {
        snprintf(buffer, bufferSize, "%s", options_[GetSelectedIndex()]);
    } else {
        snprintf(buffer, bufferSize, "--");
    }
}

void EnumParameter::SetNormalizedValueWithCurve(float normalizedValue) {
    if (optionCount_ <= 0) return;
    float curved = ApplyPotCurve(curve_, clamp(normalizedValue, 0.0f, 1.0f));
    // floor() over an equal-width slice per option, rather than rounding
    // between (optionCount_-1) points, so every option gets the same amount
    // of pot travel (the end options aren't half-width).
    int index = static_cast<int>(curved * optionCount_);
    SetSelectedIndex(clamp(index, 0, optionCount_ - 1));
}

void EnumParameter::MoveBy(int steps) {
    if (optionCount_ <= 0) return;
    int next = GetSelectedIndex() + steps;
    if (wrap_) {
        next %= optionCount_;
        if (next < 0) next += optionCount_;
    } else {
        next = ClampIndex(next, optionCount_);
    }
    currentValue_ = static_cast<float>(next);
}

void EnumParameter::Increment(int steps) {
    MoveBy(reversed_ ? -steps : steps);
}

void EnumParameter::Decrement(int steps) {
    MoveBy(reversed_ ? steps : -steps);
}

void EnumParameter::OnButtonPress() {
    MoveBy(1);
}
