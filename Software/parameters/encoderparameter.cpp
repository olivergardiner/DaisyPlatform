#include "encoderparameter.h"

#include "../hardware.h"

using namespace perspective;
using namespace daisy;

EncoderParameter::EncoderParameter(const char* name, float minValue, float maxValue, float defaultValue, float stepSize, int index, int displayIndex)
    : EffectParameter(name, minValue, maxValue, defaultValue, index, displayIndex)
    , stepSize_(stepSize)
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

void EncoderParameter::Increment(int steps) {
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
    
    currentValue_ -= steps * stepSize_ * accelerationMultiplier_;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

void EncoderParameter::Decrement(int steps) {
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
    
    currentValue_ += steps * stepSize_ * accelerationMultiplier_;
    currentValue_ = clamp(currentValue_, minValue_, maxValue_);
}

ParameterType EncoderParameter::GetType() const {
    return ParameterType::ENCODER;
}
