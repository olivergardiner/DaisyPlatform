#ifndef PERSPECTIVE_INTEGERPARAMETER_H
#define PERSPECTIVE_INTEGERPARAMETER_H

#include "effectparameter.h"

namespace perspective {

// A whole number within [min, max], displayed and stepped as an integer
// (e.g. a stage count). Bound to a potentiometer, the pot's curved position
// is rounded to the nearest whole value; bound to an encoder, each turn
// moves by one whole step. Always clamps at the ends.
class IntegerParameter : public EffectParameter {
public:
    IntegerParameter(const char* name, int minValue, int maxValue, int defaultValue, int displayIndex = -2);
    virtual ~IntegerParameter();

    ParameterKind GetKind() const override;
    void GetValueAsString(char* buffer, size_t bufferSize) const override;

    int GetIntValue() const;
    void SetIntValue(int value);

    void SetNormalizedValueWithCurve(float normalizedValue) override;
    void Increment(int steps = 1) override;
    void Decrement(int steps = 1) override;
};

} // namespace perspective

#endif // PERSPECTIVE_INTEGERPARAMETER_H
