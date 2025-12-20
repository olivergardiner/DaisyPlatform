#pragma once

#include "daisy_seed.h"
#include <string>
#include <vector>

#include "controls.h"

using namespace daisy;

namespace perspective {

enum class ParameterType {
    POTENTIOMETER,
    ENCODER,
    TOGGLE
};

enum class ControlType {
    POTENTIOMETER,
    ENCODER
};

enum class PotCurve {
    LIN,
    LOG,
    LOG_A,
    REVERSE_LOG,
    W_TAPER,
    SQUARED,
    CUBED
};

static inline float taperFunction(float x, float ym) {
    // From: https://electronics.stackexchange.com/questions/304692/formula-for-logarithmic-audio-taper-pot
    float c = ((1.0 / ym) - 1.0);
    float b = c * c;

    // Comment out this sanity check for speed - ym must not be 0.5
    /*if (b == 1.0) {
        return x; // Linear case
    }*/

    float a = 1.0 / (b - 1.0);

    return a * std::pow(b, x) - a;
}

// Base class for effect parameters
// NB: The index is used to map parameters to physical controls, and does not indicate the order of the parameter itself.
// For PotentiometerParameter and EncoderParameter, index corresponds to a physical potentiometer or encoder number respectively.
class EffectParameter {
public:
    EffectParameter(const std::string& name, float minValue, float maxValue, float defaultValue, int index = -1);
    virtual ~EffectParameter();

    // Getters
    float GetValue() const;
    float GetNormalizedValue() const;
    const std::string& GetName() const;
    float GetMin() const;
    float GetMax() const;
    int GetIndex() const;
    virtual ParameterType GetType() const = 0;  // Pure virtual - must be implemented

    // Setters
    void SetValue(float value);
    void SetNormalizedValue(float normalizedValue);  // Set value from 0.0 to 1.0
    void SetIndex(int index);

protected:
    std::string name_;
    float minValue_;
    float maxValue_;
    float currentValue_;
    int index_;  // Index for mapping to controls
};

// Subclass for parameters with potentiometer curves
class PotentiometerParameter : public EffectParameter {
public:
    PotentiometerParameter(const std::string& name, float minValue, float maxValue, float defaultValue, PotCurve curve = PotCurve::LIN, int index = -1);
    virtual ~PotentiometerParameter();
    
    void SetCurve(PotCurve curve);
    PotCurve GetCurve() const;
    
    // Set value with curve applied (input is 0.0 to 1.0)
    void SetNormalizedValueWithCurve(float normalizedValue);
    
    // Convert value to integer with configurable maximum
    int GetValueAsInt(int maxInt) const;
    
    ParameterType GetType() const override;

private:
    float ApplyCurve(float normalizedValue);
    
    PotCurve curve_;
};

// Subclass for encoder-based parameters with step increments
class EncoderParameter : public EffectParameter {
public:
    EncoderParameter(const std::string& name, float minValue, float maxValue, float defaultValue, float stepSize = 0.01f, int index = -1);
    virtual ~EncoderParameter();

    void SetStepSize(float stepSize);
    float GetStepSize() const;
    
    // Increment/decrement by step amount
    void Increment(int steps = 1);
    void Decrement(int steps = 1);
    
    ParameterType GetType() const override;

private:
    float stepSize_;
};

// Subclass for toggle parameters (on/off switches)
class ToggleParameter : public EffectParameter {
public:
    ToggleParameter(const std::string& name, bool defaultValue = false, int index = -1);
    virtual ~ToggleParameter();

    // Toggle the state
    void Toggle();
    
    // Get boolean state
    bool GetState() const;
    
    // Set boolean state
    void SetState(bool state);
    
    ParameterType GetType() const override;

private:
    bool state_;
};

} // namespace perspective
