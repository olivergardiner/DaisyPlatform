#ifndef PERSPECTIVE_ENUMPARAMETER_H
#define PERSPECTIVE_ENUMPARAMETER_H

#include "effectparameter.h"
#include <initializer_list>

namespace perspective {

// A named discrete option, stored as an index [0, count). Doubles as a
// boolean toggle by using a 2-option list (e.g. {"Off", "On"}); a button
// binding cycles through options, which is exactly a toggle for two options.
//
// Bound to a potentiometer, the pot's normalized position (after its curve)
// is quantized to the nearest option. Bound to an encoder or a button, each
// turn/press moves by one option, wrapping or clamping at the ends per
// SetWrap (defaults to wrap).
class EnumParameter : public EffectParameter {
public:
    // options must outlive the parameter (effects typically pass a static
    // array, e.g. a shared glyph/name table).
    EnumParameter(const char* name, const char** options, int count, int defaultIndex = 0, int displayIndex = -2);
    EnumParameter(const char* name, std::initializer_list<const char*> options, int defaultIndex = 0, int displayIndex = -2);
    virtual ~EnumParameter();

    ParameterKind GetKind() const override;
    void GetValueAsString(char* buffer, size_t bufferSize) const override;

    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);
    int GetOptionCount() const;

    void SetWrap(bool wrap);
    bool GetWrap() const;

    void SetNormalizedValueWithCurve(float normalizedValue) override;
    void Increment(int steps = 1) override;
    void Decrement(int steps = 1) override;
    void OnButtonPress() override;

private:
    void MoveBy(int steps);

    const char** options_;
    int optionCount_;
    bool wrap_;
    bool ownsOptions_;
};

} // namespace perspective

#endif // PERSPECTIVE_ENUMPARAMETER_H
