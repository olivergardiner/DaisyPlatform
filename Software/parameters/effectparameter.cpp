#include "effectparameter.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

#include "../hardware.h"

using namespace daisy;
using namespace perspective;

// ========== EffectParameter Base Class ==========

EffectParameter::EffectParameter(const char* name, float minValue, float maxValue, float defaultValue, int index, int displayIndex)
    : name_(nullptr)
    , minValue_(minValue)
    , maxValue_(maxValue)
    , currentValue_(defaultValue)
    , index_(index)
    , displayIndex_(displayIndex)
    , displayType_(DisplayType::DEFAULT)
    , scaleFactor_(1.0f)
    , discreteValues_(nullptr)
    , discreteValueCount_(0)
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

int EffectParameter::GetIndex() const {
    return index_;
}

int EffectParameter::GetDisplayIndex() const {
    return displayIndex_;
}

void EffectParameter::SetValue(float value) {
    currentValue_ = clamp(value, minValue_, maxValue_);
}

void EffectParameter::SetNormalizedValue(float normalizedValue) {
    float clampedNormalized = clamp(normalizedValue, 0.0f, 1.0f);
    currentValue_ = minValue_ + (clampedNormalized * (maxValue_ - minValue_));
}

void EffectParameter::SetIndex(int index) {
    index_ = index;
}

void EffectParameter::SetDisplayIndex(int displayIndex) {
    displayIndex_ = displayIndex;
}

void EffectParameter::SetDisplayType(DisplayType type) {
    displayType_ = type;
}

void EffectParameter::SetScaleFactor(float scaleFactor) {
    scaleFactor_ = scaleFactor;
}

void EffectParameter::SetDiscreteValues(const char** values, int count) {
    for (int i = 0; i < count; ++i) {
        Hardware::PrintLine("EffectParameter '%s' discrete value %d: %s", GetName(), i, values[i]);
    }
    discreteValues_ = values;
    discreteValueCount_ = count;
}

void EffectParameter::SetRange(float minValue, float maxValue) {
    if (maxValue <= minValue) {
        return;
    }

    minValue_ = minValue;
    maxValue_ = maxValue;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

int EffectParameter::GetValueAsInt(int maxInt) const {
    // Get normalized value (0.0 to 1.0)
    float normalized = GetNormalizedValue();
    
    // Scale to integer range (0 to maxInt)
    // Add 0.5 for proper rounding
    int intValue = static_cast<int>(normalized * maxInt + 0.5f);
    
    // Clamp to ensure we stay within bounds
    return clamp(intValue, 0, maxInt);
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
        
        case DisplayType::DISCRETE: {
            if (discreteValues_ != nullptr && discreteValueCount_ > 0) {
                int index = GetValueAsInt(discreteValueCount_ - 1);
                index = clamp(index, 0, discreteValueCount_ - 1);
                Hardware::PrintLine("EffectParameter '%s' discrete value index: %d", GetName(), index);
                snprintf(buffer, bufferSize, "%s", discreteValues_[index]);
            } else {
                snprintf(buffer, bufferSize, "--");
            }
            break;
        }
        
        case DisplayType::DEFAULT:
        default: {
            float value = GetValue();
            snprintf(buffer, bufferSize, "%.2f", value);
            break;
        }
    }
}
