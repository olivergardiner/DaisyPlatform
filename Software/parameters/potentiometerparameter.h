#ifndef PERSPECTIVE_POTENTIOMETERPARAMETER_H
#define PERSPECTIVE_POTENTIOMETERPARAMETER_H

#include "effectparameter.h"

namespace perspective {

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

// Subclass for parameters with potentiometer curves
class PotentiometerParameter : public EffectParameter {
public:
    PotentiometerParameter(const char* name, float minValue, float maxValue, float defaultValue, PotCurve curve = PotCurve::LIN, int index = -1, int displayIndex = -2);
    virtual ~PotentiometerParameter();
    
    void SetCurve(PotCurve curve);
    PotCurve GetCurve() const;
    
    // Set value with curve applied (input is 0.0 to 1.0)
    void SetNormalizedValueWithCurve(float normalizedValue);
    
    ParameterType GetType() const override;

private:
    float ApplyCurve(float normalizedValue);
    
    PotCurve curve_;
};

} // namespace perspective

#endif // PERSPECTIVE_POTENTIOMETERPARAMETER_H
