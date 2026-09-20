#ifndef PERSPECTIVE_VALUEPARAMETER_H
#define PERSPECTIVE_VALUEPARAMETER_H

#include "effectparameter.h"

namespace perspective {

// A continuous float parameter within [min, max]. All stepping, curve, and
// display behavior comes from EffectParameter's defaults; bind it to a
// potentiometer, an encoder, or leave it unbound as appropriate.
class ValueParameter : public EffectParameter {
public:
    ValueParameter(const char* name, float minValue, float maxValue, float defaultValue, int displayIndex = -2);
    virtual ~ValueParameter();

    ParameterKind GetKind() const override;
};

} // namespace perspective

#endif // PERSPECTIVE_VALUEPARAMETER_H
