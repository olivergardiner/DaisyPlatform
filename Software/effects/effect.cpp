#include "effect.h"

using namespace perspective;

Effect::Effect(const char* name)
    : name_(name)
    , enabled_(true)
    , wetOnly_(false)
    , metronomeEnabled_(false)
    , sampleRate_(48000.0f)
    , tempo_(0.0f)
{
}

Effect::~Effect() {
    for (auto* p : parameters_) {
        delete p;
    }
}

void Effect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    // Default implementation: process left and right channels separately
    Process(inL, outL, size);
    Process(inR, outR, size);
}

void Effect::SetTempo(float tempo) {
    tempo_ = tempo;
}

void Effect::SetMetronomeEnabled(bool enabled) {
    metronomeEnabled_ = enabled;
}

bool Effect::GetMetronomeEnabled() const {
    return metronomeEnabled_;
}

void Effect::SetMetronomeLevel(float level) {
    // Base class no-op; TempoEffect overrides
}

float Effect::GetTempoPulseBrightness() const {
    return 0.0f;
}

bool Effect::UsesExpressionPedal() const {
    return false;
}

void Effect::OnSelected() {
}

void Effect::OnDeselected() {
}

void Effect::AddParameter(EffectParameter* param) {
    if (param) {
        // Auto-assign displayIndex if not explicitly set (i.e., if it's -2)
        if (param->GetDisplayIndex() == -2) {
            // Find the highest displayIndex currently in use (excluding -1 and -2)
            int maxDisplayIndex = -1;
            for (const auto* p : parameters_) {
                if (p && p->GetDisplayIndex() >= 0 && p->GetDisplayIndex() > maxDisplayIndex) {
                    maxDisplayIndex = p->GetDisplayIndex();
                }
            }
            // Assign the next sequential displayIndex
            param->SetDisplayIndex(maxDisplayIndex + 1);
        }
        parameters_.push_back(param);
    }
}

const char* Effect::GetName() const {
    return name_;
}

void Effect::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

bool Effect::IsEnabled() const {
    return enabled_;
}

void Effect::SetWetOnly(bool wetOnly) {
    wetOnly_ = wetOnly;
}

bool Effect::IsWetOnly() const {
    return wetOnly_;
}

EffectParameter* Effect::GetParameter(size_t index) {
    if (index < parameters_.size()) {
        return parameters_[index];
    }
    return nullptr;
}

size_t Effect::GetParameterCount() const {
    return parameters_.size();
}

void Effect::SetDisplayUpdateCallback(std::function<void(EffectParameter*, size_t)> callback) {
    displayUpdateCallback_ = callback;
}

void Effect::RequestParameterDisplayUpdate(size_t parameterIndex) {
    if (displayUpdateCallback_ && parameterIndex < parameters_.size()) {
        EffectParameter* param = parameters_[parameterIndex];
        // Only trigger callback if parameter is visible (displayIndex >= 0)
        if (param && param->GetDisplayIndex() >= 0) {
            displayUpdateCallback_(param, param->GetDisplayIndex());
        }
    }
}

bool Effect::HasTempoMode() const {
    return false;
}
