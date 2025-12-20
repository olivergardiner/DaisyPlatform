#include "effect.h"
#include <cstring>

using namespace perspective;

Effect::Effect(const std::string& name)
    : name_(name)
    , enabled_(true)
    , sampleRate_(48000.0f)
    , tempo_(0.0f)
{}

Effect::~Effect() {}

void Effect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    // Default implementation: process left and right channels separately
    Process(inL, outL, size);
    Process(inR, outR, size);
}

void Effect::SetTempo(float tempoHz) {
    tempo_ = tempoHz;
    
    // Find and update rate parameters
    for (size_t i = 0; i < parameters_.size(); i++) {
        if (parameters_[i]->GetName() == "Rate") {
            parameters_[i]->SetValue(tempoHz);
            Update();
            break;
        }
    }
}

void Effect::AddParameter(EffectParameter* param) {
    if (param) {
        parameters_.push_back(param);
    }
}

const std::string& Effect::GetName() const {
    return name_;
}

void Effect::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

bool Effect::IsEnabled() const {
    return enabled_;
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
