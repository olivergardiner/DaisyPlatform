#include "encoderparameter.h"

#include "../hardware.h"

using namespace perspective;
using namespace daisy;

EncoderParameter::EncoderParameter(const char* name, float minValue, float maxValue, float defaultValue, float stepSize, int index, int displayIndex)
    : EffectParameter(name, minValue, maxValue, defaultValue, index, displayIndex)
    , stepSize_(stepSize)
    , reversed_(false)
    , lastTurnTime_(0)
    , accelerationMultiplier_(1.0f)
{}

EncoderParameter::~EncoderParameter() {}

void EncoderParameter::SetStepSize(float stepSize) {
    stepSize_ = stepSize;
}

float EncoderParameter::GetStepSize() const {
    return stepSize_;
}

void EncoderParameter::SetReversed(bool reversed) {
    reversed_ = reversed;
}

bool EncoderParameter::IsReversed() const {
    return reversed_;
}

void EncoderParameter::Increment(int steps) {
    // If reversed, swap direction without recursion
    if (reversed_) {
        if (displayType_ == DisplayType::DISCRETE && discreteValueCount_ > 0) {
            DecrementDiscrete(steps);
            return;
        }
        uint32_t now = System::GetNow();
        if (now - lastTurnTime_ <= ACCELERATION_THRESHOLD_MS && lastTurnTime_ > 0) {
            accelerationMultiplier_ += ACCELERATION_INCREMENT;
            if (accelerationMultiplier_ > MAX_ACCELERATION) accelerationMultiplier_ = MAX_ACCELERATION;
        } else {
            accelerationMultiplier_ = 1.0f;
        }
        lastTurnTime_ = now;
        currentValue_ -= steps * stepSize_ * accelerationMultiplier_;
        currentValue_ = clamp(currentValue_, minValue_, maxValue_);
        return;
    }
    // For discrete parameters, use discrete increment logic
    if (displayType_ == DisplayType::DISCRETE && discreteValueCount_ > 0) {
        IncrementDiscrete(steps);
        return;
    }
    
    // For continuous parameters, use continuous increment logic
    uint32_t now = System::GetNow();
    
    // Check if this is a continuous turn (within threshold)
    if (now - lastTurnTime_ <= ACCELERATION_THRESHOLD_MS && lastTurnTime_ > 0) {
        Hardware::PrintLine("Continuous turn detected for parameter '%s'. Increasing acceleration multiplier.", GetName());
        // Increase acceleration multiplier
        accelerationMultiplier_ += ACCELERATION_INCREMENT;
        if (accelerationMultiplier_ > MAX_ACCELERATION) {
            accelerationMultiplier_ = MAX_ACCELERATION;
        }
    } else {
        // Reset acceleration if there was a pause
        accelerationMultiplier_ = 1.0f;
    }
    
    lastTurnTime_ = now;
    
    // Inverted direction: + instead of -
    currentValue_ += steps * stepSize_ * accelerationMultiplier_;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EncoderParameter::Decrement(int steps) {
    // If reversed, swap direction without recursion
    if (reversed_) {
        if (displayType_ == DisplayType::DISCRETE && discreteValueCount_ > 0) {
            IncrementDiscrete(steps);
            return;
        }
        uint32_t now = System::GetNow();
        if (now - lastTurnTime_ <= ACCELERATION_THRESHOLD_MS && lastTurnTime_ > 0) {
            accelerationMultiplier_ += ACCELERATION_INCREMENT;
            if (accelerationMultiplier_ > MAX_ACCELERATION) accelerationMultiplier_ = MAX_ACCELERATION;
        } else {
            accelerationMultiplier_ = 1.0f;
        }
        lastTurnTime_ = now;
        currentValue_ += steps * stepSize_ * accelerationMultiplier_;
        currentValue_ = clamp(currentValue_, minValue_, maxValue_);
        return;
    }
    // For discrete parameters, use discrete decrement logic
    if (displayType_ == DisplayType::DISCRETE && discreteValueCount_ > 0) {
        DecrementDiscrete(steps);
        return;
    }
    
    // For continuous parameters, use continuous decrement logic
    uint32_t now = System::GetNow();
    
    // Check if this is a continuous turn (within threshold)
    if (now - lastTurnTime_ <= ACCELERATION_THRESHOLD_MS && lastTurnTime_ > 0) {
        Hardware::PrintLine("Continuous turn detected for parameter '%s'. Increasing acceleration multiplier.", GetName());
        accelerationMultiplier_ += ACCELERATION_INCREMENT;
        if (accelerationMultiplier_ > MAX_ACCELERATION) {
            accelerationMultiplier_ = MAX_ACCELERATION;
        }
    } else {
        // Reset acceleration if there was a pause
        accelerationMultiplier_ = 1.0f;
    }
    
    lastTurnTime_ = now;
    
    // Inverted direction: - instead of +
    currentValue_ -= steps * stepSize_ * accelerationMultiplier_;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EncoderParameter::IncrementDiscrete(int steps) {
    // For discrete parameters: move to next option(s) with wrapping
    if (discreteValueCount_ <= 0) return;
    
    // Get current index in discrete values
    int currentIndex = GetValueAsInt(discreteValueCount_ - 1);
    
    // Increment with wrapping
    int newIndex = (currentIndex + steps) % discreteValueCount_;
    if (newIndex < 0) {
        newIndex += discreteValueCount_;  // Handle negative modulo
    }
    
    Hardware::PrintLine("DiscreteIncrement parameter '%s': %d -> %d", GetName(), currentIndex, newIndex);
    
    // Convert index back to float value
    if (discreteValueCount_ > 1) {
        float normalizedValue = static_cast<float>(newIndex) / (discreteValueCount_ - 1);
        currentValue_ = minValue_ + (normalizedValue * (maxValue_ - minValue_));
    } else {
        currentValue_ = minValue_;
    }
}

void EncoderParameter::DecrementDiscrete(int steps) {
    // For discrete parameters: move to previous option(s) with wrapping
    if (discreteValueCount_ <= 0) return;
    
    // Get current index in discrete values
    int currentIndex = GetValueAsInt(discreteValueCount_ - 1);
    
    // Decrement with wrapping
    int newIndex = (currentIndex - steps) % discreteValueCount_;
    if (newIndex < 0) {
        newIndex += discreteValueCount_;  // Handle negative modulo
    }
    
    Hardware::PrintLine("DiscreteDecrement parameter '%s': %d -> %d", GetName(), currentIndex, newIndex);
    
    // Convert index back to float value
    if (discreteValueCount_ > 1) {
        float normalizedValue = static_cast<float>(newIndex) / (discreteValueCount_ - 1);
        currentValue_ = minValue_ + (normalizedValue * (maxValue_ - minValue_));
    } else {
        currentValue_ = minValue_;
    }
}

ParameterType EncoderParameter::GetType() const {
    return ParameterType::ENCODER;
}
