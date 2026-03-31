#include "timeparameter.h"
#include <cstdio>
#include <cstring>

using namespace perspective;

TimeParameter::TimeParameter(const char* name, float minValue, float maxValue, float defaultValue, float stepSize, int index, const char* tempoModeName, int displayIndex)
    : EncoderParameter(name, minValue, maxValue, defaultValue, stepSize, index, displayIndex)
    , displayMode_(TimeDisplayMode::TIME_MS)
    , timeModeName_(nullptr)
    , tempoModeName_(nullptr)
{
    // Store the original name as the time mode name
    if (name) {
        size_t len = strlen(name);
        timeModeName_ = new char[len + 1];
        strcpy(timeModeName_, name);
    }
    
    // Store optional tempo mode name if provided
    if (tempoModeName) {
        size_t len = strlen(tempoModeName);
        tempoModeName_ = new char[len + 1];
        strcpy(tempoModeName_, tempoModeName);
    }
    
    // Initialize name based on default mode
    UpdateNameForMode();
}

TimeParameter::~TimeParameter() {
    if (timeModeName_) {
        delete[] timeModeName_;
        timeModeName_ = nullptr;
    }
    if (tempoModeName_) {
        delete[] tempoModeName_;
        tempoModeName_ = nullptr;
    }
}

void TimeParameter::SetDisplayMode(TimeDisplayMode mode) {
    displayMode_ = mode;
    UpdateNameForMode();
}

TimeDisplayMode TimeParameter::GetDisplayMode() const {
    return displayMode_;
}

void TimeParameter::Increment(int steps) {
    if (displayMode_ == TimeDisplayMode::TEMPO_BPM) {
        float currentBpm = GetValueAsBPM();
        // Reversed: CW increases BPM; normal: CW decreases BPM (increases ms)
        float newBpm = IsReversed()
            ? currentBpm + (BPM_STEP_SIZE * steps)
            : currentBpm - (BPM_STEP_SIZE * steps);
        newBpm = clamp(newBpm, 30.0f, 200.0f);
        SetValue(60000.0f / newBpm);
    } else {
        // In time mode, base class handles reversed_ flag
        EncoderParameter::Increment(steps);
    }
}

void TimeParameter::Decrement(int steps) {
    if (displayMode_ == TimeDisplayMode::TEMPO_BPM) {
        float currentBpm = GetValueAsBPM();
        // Reversed: CCW decreases BPM; normal: CCW increases BPM (decreases ms)
        float newBpm = IsReversed()
            ? currentBpm - (BPM_STEP_SIZE * steps)
            : currentBpm + (BPM_STEP_SIZE * steps);
        newBpm = clamp(newBpm, 30.0f, 200.0f);
        SetValue(60000.0f / newBpm);
    } else {
        // In time mode, base class handles reversed_ flag
        EncoderParameter::Decrement(steps);
    }
}

void TimeParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }
    
    float value = GetValue();
    
    switch (displayMode_) {
        case TimeDisplayMode::TIME_MS:
            // Display as milliseconds
            snprintf(buffer, bufferSize, "%.0f", value);
            break;
            
        case TimeDisplayMode::TEMPO_BPM: {
            // Convert milliseconds to BPM (quarter note basis: BPM = 60000 / ms)
            if (value > 0.0f) {
                float bpm = 60000.0f / value;
                snprintf(buffer, bufferSize, "%.1f", bpm);
            } else {
                snprintf(buffer, bufferSize, "--");
            }
            break;
        }
        
        default:
            // Fallback to milliseconds
            snprintf(buffer, bufferSize, "%.0f ms", value);
            break;
    }
}

float TimeParameter::GetValueAsBPM() const {
    float ms = GetValue();
    if (ms > 0.0f) {
        return 60000.0f / ms;
    }
    return 0.0f;
}

float TimeParameter::GetValueAsMs() const {
    // Value is already stored as milliseconds
    return GetValue();
}

void TimeParameter::UpdateNameForMode() {
    const char* newName = nullptr;
    
    // Determine which name to use based on mode
    if (displayMode_ == TimeDisplayMode::TIME_MS) {
        // Use original time mode name
        newName = timeModeName_;
    } else if (displayMode_ == TimeDisplayMode::TEMPO_BPM) {
        // Use tempo mode name if provided, otherwise fall back to time mode name
        newName = tempoModeName_ ? tempoModeName_ : timeModeName_;
    }
    
    // Update the base class name if we have a name for this mode
    if (newName && name_) {
        // Free existing name
        delete[] name_;
        
        // Allocate and copy new name
        size_t len = strlen(newName);
        name_ = new char[len + 1];
        strcpy(name_, newName);
    }
}
