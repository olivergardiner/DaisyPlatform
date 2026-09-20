#ifndef PERSPECTIVE_EFFECTPARAMETER_H
#define PERSPECTIVE_EFFECTPARAMETER_H

#include "daisy_seed.h"
#include <cstddef>
#include <cstdint>

using namespace daisy;

namespace perspective {

// Local clamp function to avoid std::clamp (C++17)
template<typename T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// What a parameter's value means and how it is stepped/displayed.
enum class ParameterKind {
    VALUE,      // Continuous float within [min, max]
    ENUM,       // Named discrete option, stored as an index [0, count)
    TIME,       // Millisecond duration, optionally edited/displayed as tempo (BPM)
    INTEGER     // Whole number within [min, max]
};

// Which physical control, if any, drives a parameter, set independently of
// its ParameterKind via BindPotentiometer/BindEncoder/BindButton.
enum class ControlBinding {
    NONE,
    POTENTIOMETER,
    ENCODER,
    BUTTON
};

enum class DisplayType {
    DEFAULT,
    SCALED
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

// Applies a potentiometer taper to a normalized (0..1) value.
float ApplyPotCurve(PotCurve curve, float normalizedValue);

// Semantic macro role for parameters that are statically mapped to the
// dedicated macro potentiometers (Mix/Depth/Rate/Feedback), plus Subdivision
// which stays encoder- or pot-controlled but is grouped with the other macros.
// Effects with two instances of the same macro (e.g. dual delay lines) tag
// the first instance as primary (macro-pot controllable) and the second as
// non-primary (encoder select/edit only).
enum class MacroRole {
    NONE,
    MIX,
    DEPTH,
    RATE,
    FEEDBACK,
    SUBDIVISION
};

// Base class for effect parameters.
//
// A parameter's ParameterKind (Value/Enum/Time/Integer) defines what its
// value means and how it is stepped and displayed. Independently, a
// ControlBinding (Potentiometer/Encoder/Button/None) defines which physical
// control - if any - drives it, set via BindPotentiometer/BindEncoder/
// BindButton when the effect builds its parameter list. The two are
// orthogonal: any kind can be bound to any control that makes sense for it.
//
// NB: The displayIndex is used to determine the display order (0-based).
// displayIndex -1 = hidden (not displayed), -2 = auto-assign (default).
class EffectParameter {
public:
    EffectParameter(const char* name, float minValue, float maxValue, float defaultValue, int displayIndex = -2);
    virtual ~EffectParameter();

    // Getters
    float GetValue() const;
    float GetNormalizedValue() const;
    virtual const char* GetName() const;
    float GetMin() const;
    float GetMax() const;
    int GetDisplayIndex() const;
    virtual ParameterKind GetKind() const = 0;
    virtual void GetValueAsString(char* buffer, size_t bufferSize) const;
    DisplayType GetDisplayType() const;

    // Display type configuration
    void SetDisplayType(DisplayType type);
    void SetScaleFactor(float scaleFactor);
    void SetRange(float minValue, float maxValue);

    // Setters
    virtual void SetValue(float value);
    void SetNormalizedValue(float normalizedValue);  // Set value from 0.0 to 1.0
    void SetDisplayIndex(int displayIndex);

    // Macro role tagging (used to statically map the first instance of a
    // Mix/Depth/Rate/Feedback parameter to its dedicated macro potentiometer)
    void SetMacroRole(MacroRole role, bool isPrimary = true);
    MacroRole GetMacroRole() const;
    bool IsMacroPrimary() const;

    // ---- Control binding ----
    // Exactly one of these applies per parameter; effects call the one that
    // matches the physical control they want to drive this parameter with.
    void BindPotentiometer(int index, PotCurve curve = PotCurve::LIN);
    void BindEncoder(int index, float stepSize = 0.01f, bool reversed = false);
    void BindButton(int index);
    ControlBinding GetControlBinding() const;
    int GetIndex() const;  // Physical control index for the current binding, or -1 if unbound

    PotCurve GetCurve() const;
    void SetCurve(PotCurve curve);
    float GetStepSize() const;
    void SetStepSize(float stepSize);
    bool IsReversed() const;
    void SetReversed(bool reversed);

    // ---- Uniform control-driven interface ----
    // Called by the UI layer when a bound potentiometer moves. Default
    // applies the bound curve across [min, max]; kinds whose value isn't a
    // plain curved float (Enum) override this to quantize to an option.
    virtual void SetNormalizedValueWithCurve(float normalizedValue);

    // Called by the UI layer for relative encoder motion, and for the
    // generic "edit selected parameter" path regardless of binding.
    virtual void Increment(int steps = 1);
    virtual void Decrement(int steps = 1);

    // Called by the UI layer when a bound button is pressed. Default is a
    // no-op; Enum overrides this to cycle to the next option.
    virtual void OnButtonPress();

protected:
    char* name_;
    float minValue_;
    float maxValue_;
    float currentValue_;
    int displayIndex_;
    DisplayType displayType_;
    float scaleFactor_;
    MacroRole macroRole_;
    bool macroPrimary_;

    ControlBinding controlBinding_;
    int controlIndex_;
    PotCurve curve_;
    float stepSize_;
    bool reversed_;

private:
    uint32_t lastTurnTime_;
    float accelerationMultiplier_;

    static constexpr uint32_t ACCELERATION_THRESHOLD_MS = 300; // Time window for continuous turning
    static constexpr float MAX_ACCELERATION = 50.0f; // Maximum speed multiplier
    static constexpr float ACCELERATION_INCREMENT = 2.0f; // How much to increase multiplier per turn

    // Shared acceleration bookkeeping for the default (continuous) Increment/Decrement.
    float NextAcceleratedStep();
};

} // namespace perspective

#endif // PERSPECTIVE_EFFECTPARAMETER_H
