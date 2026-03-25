#ifndef PERSPECTIVE_EFFECTPARAMETER_H
#define PERSPECTIVE_EFFECTPARAMETER_H

#include "daisy_seed.h"
#include <vector>

#include "../controls.h"

using namespace daisy;

namespace perspective {

// Local clamp function to avoid std::clamp (C++17)
template<typename T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

enum class ParameterType {
    POTENTIOMETER,
    ENCODER,
    TOGGLE
};

enum class DisplayType {
    DEFAULT,
    SCALED,
    DISCRETE
};

enum class ControlType {
    POTENTIOMETER,
    ENCODER
};

// Base class for effect parameters
// NB: The index is used to map parameters to physical controls, and does not indicate the order of the parameter itself.
// For PotentiometerParameter and EncoderParameter, index corresponds to a physical potentiometer or encoder number respectively.
// The displayIndex is used to determine the display order (0-based). 
// displayIndex -1 = hidden (not displayed), -2 = auto-assign (default).
class EffectParameter {
public:
    EffectParameter(const char* name, float minValue, float maxValue, float defaultValue, int index = -1, int displayIndex = -2);
    virtual ~EffectParameter();

    // Getters
    float GetValue() const;
    float GetNormalizedValue() const;
    const char* GetName() const;
    float GetMin() const;
    float GetMax() const;
    int GetIndex() const;
    int GetDisplayIndex() const;
    virtual ParameterType GetType() const = 0;  // Pure virtual - must be implemented
    virtual void GetValueAsString(char* buffer, size_t bufferSize) const;
    
    // Display type configuration
    void SetDisplayType(DisplayType type);
    void SetScaleFactor(float scaleFactor);
    void SetDiscreteValues(const char** values, int count);
    void SetRange(float minValue, float maxValue);

    // Setters
    virtual void SetValue(float value);
    void SetNormalizedValue(float normalizedValue);  // Set value from 0.0 to 1.0
    void SetIndex(int index);
    void SetDisplayIndex(int displayIndex);
    void SetUseBravura(bool useBravura);
    
    // Bravura font control
    bool GetUseBravura() const;
    
    // Convert value to integer with configurable maximum
    int GetValueAsInt(int maxInt) const;

protected:
    char* name_;
    float minValue_;
    float maxValue_;
    float currentValue_;
    int index_;  // Index for mapping to controls
    int displayIndex_;  // Index for display order (-1 = hidden)
    DisplayType displayType_;
    float scaleFactor_;
    const char **discreteValues_;
    int discreteValueCount_;
};

} // namespace perspective

#endif // PERSPECTIVE_EFFECTPARAMETER_H