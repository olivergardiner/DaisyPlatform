#ifndef PERSPECTIVE_COMPOUNDEFFECT_H
#define PERSPECTIVE_COMPOUNDEFFECT_H

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
    CompoundEffect(const char* name, RoutingMode mode);
    virtual ~CompoundEffect() override;

    void Init(float sampleRate) override;
    void Process(const float* in, float* out, size_t size) override;
    void ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    void SetTempo(float tempoHz) override;
    void SetMetronomeEnabled(bool enabled) override;
    void SetMetronomeLevel(float level) override;
    float GetTempoPulseBrightness() const override;
    bool HasTempoMode() const override;
    void OnSelected() override;
    void OnDeselected() override;

protected:
    // Add an effect to the compound effect
    void AddEffect(Effect* effect);
    
    // Get child effects
    const std::vector<Effect*>& GetEffects() const;
    
    // Set/get whether to pass clean signal through in parallel mode
    void SetPassClean(bool passClean);
    bool GetPassClean() const;
    
    // Set/get whether to scale parallel effects output
    void SetScaleParallel(bool scaleParallel);
    bool GetScaleParallel() const;
    
    // Routing mode
    RoutingMode routingMode_;
    
    // Pass clean signal through in parallel mode (default true)
    bool passClean_;
    
    // Scale parallel effects output by 1/N (default false)
    bool scaleParallel_;
    
    // Child effects
    std::vector<Effect*> effects_;
    
    // Temporary buffers for processing
    float* tempBufferL_;
    float* tempBufferR_;
    size_t bufferSize_;
};

} // namespace perspective

#endif // PERSPECTIVE_COMPOUNDEFFECT_H