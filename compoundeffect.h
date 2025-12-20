#pragma once

#include "effect.h"
#include <vector>

namespace perspective {

enum class RoutingMode {
    SERIES,    // Effects process in sequence (output of one feeds into next)
    PARALLEL   // Effects process in parallel (same input, outputs mixed)
};

// Abstract base class for compound effects that combine multiple effects
class CompoundEffect : public Effect {
public:
    CompoundEffect(const std::string& name, RoutingMode mode);
    virtual ~CompoundEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    void SetTempo(float tempoHz) override;

protected:
    // Add an effect to the compound effect
    void AddEffect(Effect* effect);
    
    // Get child effects
    const std::vector<Effect*>& GetEffects() const;
    
    // Routing mode
    RoutingMode routingMode_;
    
    // Child effects
    std::vector<Effect*> effects_;
    
    // Temporary buffers for processing
    float* tempBufferL_;
    float* tempBufferR_;
    size_t bufferSize_;
};

} // namespace perspective
