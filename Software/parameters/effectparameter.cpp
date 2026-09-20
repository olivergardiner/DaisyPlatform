#include "effectparameter.h"
#include <cmath>
#include <cstring>
#include <cstdio>

using namespace daisy;
using namespace perspective;

namespace {

// From: https://electronics.stackexchange.com/questions/304692/formula-for-logarithmic-audio-taper-pot
float TaperFunction(float x, float ym) {
    float c = ((1.0f / ym) - 1.0f);
    float b = c * c;
    float a = 1.0f / (b - 1.0f);
    return a * std::pow(b, x) - a;
}

} // namespace

float perspective::ApplyPotCurve(PotCurve curve, float normalizedValue) {
    switch (curve) {
        case PotCurve::LIN:
            return normalizedValue;

        case PotCurve::LOG:
            return TaperFunction(normalizedValue, 0.12f);

        case PotCurve::REVERSE_LOG:
            return TaperFunction(normalizedValue, 0.88f);

        case PotCurve::LOG_A:
            return TaperFunction(normalizedValue, 0.25f);

        case PotCurve::W_TAPER:
            // Dual curve for blend/crossfade controls - equal power transition.
            if (normalizedValue < 0.5f) {
                return 2.0f * normalizedValue * normalizedValue;
            } else {
                float inverse = 1.0f - normalizedValue;
                return 1.0f - (2.0f * inverse * inverse);
            }

        case PotCurve::SQUARED:
            return normalizedValue * normalizedValue;

        case PotCurve::CUBED:
            return normalizedValue * normalizedValue * normalizedValue;

        default:
            return normalizedValue;
    }
}

// ========== EffectParameter ==========

EffectParameter::EffectParameter(const char* name, float minValue, float maxValue, float defaultValue, int displayIndex)
    : name_(nullptr)
    , minValue_(minValue)
    , maxValue_(maxValue)
    , currentValue_(defaultValue)
    , displayIndex_(displayIndex)
    , displayType_(DisplayType::DEFAULT)
    , scaleFactor_(1.0f)
    , macroRole_(MacroRole::NONE)
    , macroPrimary_(true)
    , controlBinding_(ControlBinding::NONE)
    , controlIndex_(-1)
    , curve_(PotCurve::LIN)
    , stepSize_(0.01f)
    , reversed_(false)
    , lastTurnTime_(0)
    , accelerationMultiplier_(1.0f)
{
    if (name) {
        size_t len = strlen(name);
        name_ = new char[len + 1];
        strcpy(name_, name);
    }
}

EffectParameter::~EffectParameter() {
    if (name_) {
        delete[] name_;
        name_ = nullptr;
    }
}

float EffectParameter::GetValue() const {
    return currentValue_;
}

float EffectParameter::GetNormalizedValue() const {
    if (maxValue_ <= minValue_) {
        return 0.0f;
    }
    return (currentValue_ - minValue_) / (maxValue_ - minValue_);
}

const char* EffectParameter::GetName() const {
    return name_;
}

float EffectParameter::GetMin() const {
    return minValue_;
}

float EffectParameter::GetMax() const {
    return maxValue_;
}

int EffectParameter::GetDisplayIndex() const {
    return displayIndex_;
}

DisplayType EffectParameter::GetDisplayType() const {
    return displayType_;
}

void EffectParameter::SetDisplayType(DisplayType type) {
    displayType_ = type;
}

void EffectParameter::SetScaleFactor(float scaleFactor) {
    scaleFactor_ = scaleFactor;
}

void EffectParameter::SetRange(float minValue, float maxValue) {
    if (maxValue <= minValue) {
        return;
    }
    minValue_ = minValue;
    maxValue_ = maxValue;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EffectParameter::SetValue(float value) {
    currentValue_ = clamp(value, minValue_, maxValue_);
}

void EffectParameter::SetNormalizedValue(float normalizedValue) {
    float clampedNormalized = clamp(normalizedValue, 0.0f, 1.0f);
    currentValue_ = minValue_ + (clampedNormalized * (maxValue_ - minValue_));
}

void EffectParameter::SetDisplayIndex(int displayIndex) {
    displayIndex_ = displayIndex;
}

void EffectParameter::SetMacroRole(MacroRole role, bool isPrimary) {
    macroRole_ = role;
    macroPrimary_ = isPrimary;
}

MacroRole EffectParameter::GetMacroRole() const {
    return macroRole_;
}

bool EffectParameter::IsMacroPrimary() const {
    return macroPrimary_;
}

void EffectParameter::BindPotentiometer(int index, PotCurve curve) {
    controlBinding_ = ControlBinding::POTENTIOMETER;
    controlIndex_ = index;
    curve_ = curve;
}

void EffectParameter::BindEncoder(int index, float stepSize, bool reversed) {
    controlBinding_ = ControlBinding::ENCODER;
    controlIndex_ = index;
    stepSize_ = stepSize;
    reversed_ = reversed;
}

void EffectParameter::BindButton(int index) {
    controlBinding_ = ControlBinding::BUTTON;
    controlIndex_ = index;
}

ControlBinding EffectParameter::GetControlBinding() const {
    return controlBinding_;
}

int EffectParameter::GetIndex() const {
    return controlIndex_;
}

PotCurve EffectParameter::GetCurve() const {
    return curve_;
}

void EffectParameter::SetCurve(PotCurve curve) {
    curve_ = curve;
}

float EffectParameter::GetStepSize() const {
    return stepSize_;
}

void EffectParameter::SetStepSize(float stepSize) {
    stepSize_ = stepSize;
}

bool EffectParameter::IsReversed() const {
    return reversed_;
}

void EffectParameter::SetReversed(bool reversed) {
    reversed_ = reversed;
}

float EffectParameter::NextAcceleratedStep() {
    uint32_t now = System::GetNow();
    if (lastTurnTime_ > 0 && now - lastTurnTime_ <= ACCELERATION_THRESHOLD_MS) {
        accelerationMultiplier_ += ACCELERATION_INCREMENT;
        if (accelerationMultiplier_ > MAX_ACCELERATION) {
            accelerationMultiplier_ = MAX_ACCELERATION;
        }
    } else {
        accelerationMultiplier_ = 1.0f;
    }
    lastTurnTime_ = now;
    return stepSize_ * accelerationMultiplier_;
}

void EffectParameter::SetNormalizedValueWithCurve(float normalizedValue) {
    float curved = ApplyPotCurve(curve_, clamp(normalizedValue, 0.0f, 1.0f));
    currentValue_ = minValue_ + (curved * (maxValue_ - minValue_));
}

void EffectParameter::Increment(int steps) {
    float delta = static_cast<float>(steps) * NextAcceleratedStep();
    currentValue_ += reversed_ ? -delta : delta;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EffectParameter::Decrement(int steps) {
    float delta = static_cast<float>(steps) * NextAcceleratedStep();
    currentValue_ += reversed_ ? delta : -delta;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EffectParameter::OnButtonPress() {
    // No-op by default; Enum overrides to cycle to the next option.
}

void EffectParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }

    switch (displayType_) {
        case DisplayType::SCALED: {
            float scaledValue = GetValue() * scaleFactor_;
            snprintf(buffer, bufferSize, "%.2f", scaledValue);
            break;
        }

        case DisplayType::DEFAULT:
        default: {
            snprintf(buffer, bufferSize, "%.2f", GetValue());
            break;
        }
    }
}
