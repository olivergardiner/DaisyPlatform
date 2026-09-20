#include "valueparameter.h"

using namespace perspective;

ValueParameter::ValueParameter(const char* name, float minValue, float maxValue, float defaultValue, int displayIndex)
    : EffectParameter(name, minValue, maxValue, defaultValue, displayIndex)
{}

ValueParameter::~ValueParameter() {}

ParameterKind ValueParameter::GetKind() const {
    return ParameterKind::VALUE;
}
