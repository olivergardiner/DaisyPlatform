#ifndef PERSPECTIVE_EFFECT_H
#define PERSPECTIVE_EFFECT_H

#include "effectparameter.h"
#include <vector>
#include <memory>
#include <string>

namespace perspective {

// Base class for audio effects
class Effect {
public:
    Effect(const std::string& name);
    virtual ~Effect();

    // Initialize the effect (called once during setup)
    // The Init method should set up any parameters to control the effect.
    // Because different parameter types map to different physical controls,
    // the index of different parameters will not be sequential.
    virtual void Init(float sampleRate) = 0;

    // Process audio - must be implemented by derived classes
    virtual void Process(float* in, float* out, size_t size) = 0;

    // Process stereo audio (default implementation calls mono Process)
    virtual void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size);

    // Update effect parameters - called when parameters change
    virtual void Update() = 0;

    // Set tempo (called from tap tempo)
    virtual void SetTempo(float tempoHz);

    // Get effect name
    const std::string& GetName() const;

    // Enable/disable the effect
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Get parameter by index
    EffectParameter* GetParameter(size_t index);
    size_t GetParameterCount() const;

protected:
    void AddParameter(EffectParameter* param);

    std::string name_;
    std::vector<EffectParameter*> parameters_;
    bool enabled_;
    float sampleRate_;
    float tempo_;
};

} // namespace perspective

#endif // PERSPECTIVE_EFFECT_H
