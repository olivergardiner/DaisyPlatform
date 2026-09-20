#include "timeparameter.h"
#include "enumparameter.h"
#include <cstdio>
#include <cstring>

using namespace perspective;

TimeParameter::TimeParameter(const char* name, float minMs, float maxMs, float defaultMs, const char* tempoModeName, int displayIndex)
    : EffectParameter(name, minMs, maxMs, defaultMs, displayIndex)
    , modeToggle_(nullptr)
    , timeModeName_(nullptr)
    , tempoModeName_(nullptr)
{
    if (name) {
        size_t len = strlen(name);
        timeModeName_ = new char[len + 1];
        strcpy(timeModeName_, name);
    }
    if (tempoModeName) {
        size_t len = strlen(tempoModeName);
        tempoModeName_ = new char[len + 1];
        strcpy(tempoModeName_, tempoModeName);
    }
}

TimeParameter::~TimeParameter() {
    delete[] timeModeName_;
    delete[] tempoModeName_;
}

ParameterKind TimeParameter::GetKind() const {
    return ParameterKind::TIME;
}

void TimeParameter::SetModeToggle(EnumParameter* modeToggle) {
    modeToggle_ = modeToggle;
}

EnumParameter* TimeParameter::GetModeToggle() const {
    return modeToggle_;
}

TimeDisplayMode TimeParameter::GetDisplayMode() const {
    if (modeToggle_ != nullptr && modeToggle_->GetSelectedIndex() == 1) {
        return TimeDisplayMode::TEMPO_BPM;
    }
    return TimeDisplayMode::TIME_MS;
}

const char* TimeParameter::GetName() const {
    if (GetDisplayMode() == TimeDisplayMode::TEMPO_BPM && tempoModeName_) {
        return tempoModeName_;
    }
    return timeModeName_ ? timeModeName_ : EffectParameter::GetName();
}

void TimeParameter::Increment(int steps) {
    if (GetDisplayMode() == TimeDisplayMode::TEMPO_BPM) {
        float currentBpm = GetValueAsBPM();
        // Reversed: CW increases BPM; normal: CW decreases BPM (increases ms)
        float newBpm = IsReversed()
            ? currentBpm + (BPM_STEP_SIZE * steps)
            : currentBpm - (BPM_STEP_SIZE * steps);
        newBpm = clamp(newBpm, 30.0f, 200.0f);
        SetValue(60000.0f / newBpm);
    } else {
        EffectParameter::Increment(steps);
    }
}

void TimeParameter::Decrement(int steps) {
    if (GetDisplayMode() == TimeDisplayMode::TEMPO_BPM) {
        float currentBpm = GetValueAsBPM();
        // Reversed: CCW decreases BPM; normal: CCW increases BPM (decreases ms)
        float newBpm = IsReversed()
            ? currentBpm - (BPM_STEP_SIZE * steps)
            : currentBpm + (BPM_STEP_SIZE * steps);
        newBpm = clamp(newBpm, 30.0f, 200.0f);
        SetValue(60000.0f / newBpm);
    } else {
        EffectParameter::Decrement(steps);
    }
}

void TimeParameter::GetValueAsString(char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }

    float value = GetValue();

    if (GetDisplayMode() == TimeDisplayMode::TEMPO_BPM) {
        if (value > 0.0f) {
            snprintf(buffer, bufferSize, "%.1f", 60000.0f / value);
        } else {
            snprintf(buffer, bufferSize, "--");
        }
    } else {
        snprintf(buffer, bufferSize, "%.0f", value);
    }
}

float TimeParameter::GetValueAsBPM() const {
    float ms = GetValue();
    return ms > 0.0f ? 60000.0f / ms : 0.0f;
}

float TimeParameter::GetValueAsMs() const {
    return GetValue();
}
