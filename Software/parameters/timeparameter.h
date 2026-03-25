#ifndef PERSPECTIVE_TIMEPARAMETER_H
#define PERSPECTIVE_TIMEPARAMETER_H

#include "encoderparameter.h"

namespace perspective {

enum class TimeDisplayMode {
    TIME_MS,    // Display as milliseconds
    TEMPO_BPM   // Display as tempo in BPM
};

// Subclass for time-based encoder parameters that can display as time or tempo
class TimeParameter : public EncoderParameter {
public:
    TimeParameter(const char* name, float minValue, float maxValue, float defaultValue, float stepSize = 0.01f, int index = -1, const char* tempoModeName = nullptr, int displayIndex = -2);
    virtual ~TimeParameter();

    // Set display mode
    void SetDisplayMode(TimeDisplayMode mode);
    TimeDisplayMode GetDisplayMode() const;
    
    // Override to handle BPM-based stepping when in tempo mode
    void Increment(int steps) override;
    void Decrement(int steps) override;
    
    // Override to format value based on display mode
    void GetValueAsString(char* buffer, size_t bufferSize) const override;
    
    // Get value as BPM (converts from ms if needed)
    float GetValueAsBPM() const;
    
    // Get value as milliseconds (converts from BPM if needed)
    float GetValueAsMs() const;

private:
    TimeDisplayMode displayMode_;
    char* timeModeName_;  // Original name (time mode)
    char* tempoModeName_; // Optional alternate name for tempo mode
    
    static constexpr float BPM_STEP_SIZE = 0.5f; // BPM increment when in tempo mode
    
    void UpdateNameForMode();
};

} // namespace perspective

#endif // PERSPECTIVE_TIMEPARAMETER_H
