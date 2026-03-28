#ifndef PERSPECTIVE_EFFECT_H
#define PERSPECTIVE_EFFECT_H

#include "../parameters/effectparameter.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/encoderparameter.h"
#include "../parameters/toggleparameter.h"
#include <vector>
#include <functional>
//#include <memory>

namespace perspective {

// Base class for audio effects
class Effect {
public:
    Effect(const char* name);
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
    virtual void SetTempo(float tempo);
    
    // Set/get metronome enabled state (global from Perspective)
    virtual void SetMetronomeEnabled(bool enabled);
    bool GetMetronomeEnabled() const;
    virtual void SetMetronomeLevel(float level);
    virtual float GetTempoPulseBrightness() const;
    virtual bool HasTempoMode() const;

    // Lifecycle hooks when an effect becomes current or is no longer current.
    virtual void OnSelected();
    virtual void OnDeselected();

    // Get effect name
    const char* GetName() const;

    // Enable/disable the effect
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    
    // Set wet-only mode (for parallel mixing in compound effects)
    void SetWetOnly(bool wetOnly);
    bool IsWetOnly() const;

    // Get parameter by index
    EffectParameter* GetParameter(size_t index);
    size_t GetParameterCount() const;
    
    // Set callback for display updates
    void SetDisplayUpdateCallback(std::function<void(EffectParameter*, size_t)> callback);

protected:
    void AddParameter(EffectParameter* param);
    
    // Request a parameter display update (calls the callback if set)
    void RequestParameterDisplayUpdate(size_t parameterIndex);

    char* name_;
    std::vector<EffectParameter*> parameters_;
    bool enabled_;
    bool wetOnly_;  // If true, output wet signal only (for parallel compound effects)
    bool metronomeEnabled_;  // Global metronome state (set by Perspective)
    float sampleRate_;
    float tempo_;
    std::function<void(EffectParameter*, size_t)> displayUpdateCallback_;
};

} // namespace perspective

#endif // PERSPECTIVE_EFFECT_H
