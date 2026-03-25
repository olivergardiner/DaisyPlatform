#ifndef PERSPECTIVE_ENCODERPARAMETER_H
#define PERSPECTIVE_ENCODERPARAMETER_H

#include "effectparameter.h"
#include "daisy_core.h"

namespace perspective {

// Subclass for encoder-based parameters with step increments
class EncoderParameter : public EffectParameter {
public:
    EncoderParameter(const char* name, float minValue, float maxValue, float defaultValue, float stepSize = 0.01f, int index = -1, int displayIndex = -2);
    virtual ~EncoderParameter();

    void SetStepSize(float stepSize);
    float GetStepSize() const;
    
    // Increment/decrement by step amount
    virtual void Increment(int steps = 1);
    virtual void Decrement(int steps = 1);
    
    ParameterType GetType() const override;

private:
    float stepSize_;
    uint32_t lastTurnTime_;
    float accelerationMultiplier_;
    
    static constexpr uint32_t ACCELERATION_THRESHOLD_MS = 300; // Time window for continuous turning
    static constexpr float MAX_ACCELERATION = 50.0f; // Maximum speed multiplier
    static constexpr float ACCELERATION_INCREMENT = 2.0f; // How much to increase multiplier per turn
};

} // namespace perspective

#endif // PERSPECTIVE_ENCODERPARAMETER_H
