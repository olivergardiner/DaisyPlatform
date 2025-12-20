#include "compoundeffect.h"
#include <cstring>
#include <algorithm>

using namespace perspective;

CompoundEffect::CompoundEffect(const std::string& name, RoutingMode mode)
    : Effect(name)
    , routingMode_(mode)
    , tempBufferL_(nullptr)
    , tempBufferR_(nullptr)
    , bufferSize_(0)
{}

CompoundEffect::~CompoundEffect() {
    // Clean up child effects
    for (Effect* effect : effects_) {
        delete effect;
    }
    effects_.clear();
    
    // Clean up temporary buffers
    if (tempBufferL_) {
        delete[] tempBufferL_;
        tempBufferL_ = nullptr;
    }
    if (tempBufferR_) {
        delete[] tempBufferR_;
        tempBufferR_ = nullptr;
    }
}

void CompoundEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Initialize all child effects
    for (Effect* effect : effects_) {
        if (effect) {
            effect->Init(sampleRate);
        }
    }
    
    // Allocate temporary buffers (assuming max 128 samples per block)
    bufferSize_ = 128;
    tempBufferL_ = new float[bufferSize_];
    tempBufferR_ = new float[bufferSize_];
}

void CompoundEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_ || effects_.empty()) {
        // Bypass - pass through dry signal
        std::memcpy(out, in, size * sizeof(float));
        return;
    }
    
    if (routingMode_ == RoutingMode::SERIES) {
        // Series processing: output of one effect feeds into next
        // First effect processes input
        if (effects_[0]) {
            effects_[0]->Process(in, out, size);
        }
        
        // Subsequent effects use temp buffer for ping-pong processing
        for (size_t i = 1; i < effects_.size(); i++) {
            if (effects_[i]) {
                // Copy current output to temp buffer
                std::memcpy(tempBufferL_, out, size * sizeof(float));
                // Process temp buffer to output
                effects_[i]->Process(tempBufferL_, out, size);
            }
        }
    } else {
        // Parallel processing: all effects process same input, outputs are mixed
        // Clear output buffer
        std::memset(out, 0, size * sizeof(float));
        
        // Process each effect and accumulate
        for (Effect* effect : effects_) {
            if (effect) {
                effect->Process(in, tempBufferL_, size);
                
                // Mix into output (equal mix for now)
                float gain = 1.0f / static_cast<float>(effects_.size());
                for (size_t i = 0; i < size; i++) {
                    out[i] += tempBufferL_[i] * gain;
                }
            }
        }
    }
}

void CompoundEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_ || effects_.empty()) {
        // Bypass - pass through dry signal
        std::memcpy(outL, inL, size * sizeof(float));
        std::memcpy(outR, inR, size * sizeof(float));
        return;
    }
    
    // Reallocate buffers if needed
    if (size > bufferSize_) {
        if (tempBufferL_) delete[] tempBufferL_;
        if (tempBufferR_) delete[] tempBufferR_;
        bufferSize_ = size;
        tempBufferL_ = new float[bufferSize_];
        tempBufferR_ = new float[bufferSize_];
    }
    
    if (routingMode_ == RoutingMode::SERIES) {
        // Series processing: output of one effect feeds into next
        // First effect processes input
        if (effects_[0]) {
            effects_[0]->ProcessStereo(inL, inR, outL, outR, size);
        }
        
        // Subsequent effects use temp buffer for ping-pong processing
        for (size_t i = 1; i < effects_.size(); i++) {
            if (effects_[i]) {
                // Copy current output to temp buffers
                std::memcpy(tempBufferL_, outL, size * sizeof(float));
                std::memcpy(tempBufferR_, outR, size * sizeof(float));
                // Process temp buffers to output
                effects_[i]->ProcessStereo(tempBufferL_, tempBufferR_, outL, outR, size);
            }
        }
    } else {
        // Parallel processing: all effects process same input, outputs are mixed
        // Clear output buffers
        std::memset(outL, 0, size * sizeof(float));
        std::memset(outR, 0, size * sizeof(float));
        
        // Process each effect and accumulate
        for (Effect* effect : effects_) {
            if (effect) {
                effect->ProcessStereo(inL, inR, tempBufferL_, tempBufferR_, size);
                
                // Mix into output (equal mix for now)
                float gain = 1.0f / static_cast<float>(effects_.size());
                for (size_t i = 0; i < size; i++) {
                    outL[i] += tempBufferL_[i] * gain;
                    outR[i] += tempBufferR_[i] * gain;
                }
            }
        }
    }
}

void CompoundEffect::Update() {
    // Update all child effects
    for (Effect* effect : effects_) {
        if (effect) {
            effect->Update();
        }
    }
}

void CompoundEffect::SetTempo(float tempoHz) {
    tempo_ = tempoHz;
    
    // Propagate tempo to all child effects
    for (Effect* effect : effects_) {
        if (effect) {
            effect->SetTempo(tempoHz);
        }
    }
}

void CompoundEffect::AddEffect(Effect* effect) {
    if (effect) {
        effects_.push_back(effect);
        
        // If already initialized, initialize the new effect
        if (sampleRate_ > 0) {
            effect->Init(sampleRate_);
        }
    }
}

const std::vector<Effect*>& CompoundEffect::GetEffects() const {
    return effects_;
}
