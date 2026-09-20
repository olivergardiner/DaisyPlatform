#ifndef PERSPECTIVE_TIMEPARAMETER_H
#define PERSPECTIVE_TIMEPARAMETER_H

#include "effectparameter.h"

namespace perspective {

class EnumParameter;

enum class TimeDisplayMode {
    TIME_MS,    // Display/edit as milliseconds
    TEMPO_BPM   // Display/edit as tempo in BPM
};

// A millisecond duration that can optionally be displayed and edited as a
// tempo in BPM. The mode is not stored here directly - it is read from a
// linked mode-toggle EnumParameter (typically a 2-option "Time"/"Tempo"
// toggle bound to a button), set via SetModeToggle. That keeps the mode as
// a single source of truth instead of two parameters kept in sync by name.
class TimeParameter : public EffectParameter {
public:
    TimeParameter(const char* name, float minMs, float maxMs, float defaultMs, const char* tempoModeName = nullptr, int displayIndex = -2);
    virtual ~TimeParameter();

    ParameterKind GetKind() const override;
    const char* GetName() const override;

    void SetModeToggle(EnumParameter* modeToggle);
    EnumParameter* GetModeToggle() const;
    TimeDisplayMode GetDisplayMode() const;

    // Overridden to handle BPM-aware stepping while in tempo mode; defers to
    // the base class's continuous stepping otherwise.
    void Increment(int steps = 1) override;
    void Decrement(int steps = 1) override;

    void GetValueAsString(char* buffer, size_t bufferSize) const override;

    // Get value as BPM (converts from ms)
    float GetValueAsBPM() const;
    // Get value as milliseconds (value is already stored this way)
    float GetValueAsMs() const;

private:
    EnumParameter* modeToggle_;
    char* timeModeName_;  // Name shown/used in time mode
    char* tempoModeName_; // Optional alternate name shown/used in tempo mode

    static constexpr float BPM_STEP_SIZE = 0.5f; // BPM increment when in tempo mode
};

} // namespace perspective

#endif // PERSPECTIVE_TIMEPARAMETER_H
