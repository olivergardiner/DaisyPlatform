#ifndef PERSPECTIVE_PERSPECTIVE_H
#define PERSPECTIVE_PERSPECTIVE_H

#include "hardware.h"
#include "parameters/effectparameter.h"
#include "parameters/potentiometerparameter.h"
#include "parameters/encoderparameter.h"
#include "parameters/toggleparameter.h"
#include "ui/ui.h"
#include "effects/tunereffect.h"

#include <vector>

namespace perspective {

class Effect;  // Forward declaration

enum class BypassType {
    TRUE_BYPASS,    // Hardware bypass via relay (drive pin low)
    PASSTHROUGH     // Pass audio through unprocessed
};

class Perspective : public UI {
public:
    Perspective();
    ~Perspective() override;
    
    void Init() override;
    void Exec() override;    
    void AudioCallbackImpl(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);
    void SetCurrentEffect(size_t index);
    
protected:
    void RegisterEventListeners();
    void LoadEffects();
    void ToggleBypass();
    void ToggleBypassType();
    void HandleTapTempo();
    void ToggleMetronome();
    void EnterTunerMode();
    void ExitTunerMode();
    void UpdateTunerDisplay();
    void UpdateParameterDisplay(EffectParameter* param, size_t displayIndex);
    void UpdateStatusDisplay();
    
    Hardware hardware;
    Effect* currentEffect_ = nullptr;
    std::vector<Effect*> effects_;
    size_t currentEffectIndex_ = 0;

    bool bypassMode_ = false;
    BypassType bypassType_ = BypassType::PASSTHROUGH;
    bool tunerMode_ = false;
    TunerEffect* tunerEffect_ = nullptr;
    bool metronomeEnabled_ = false;  // Global metronome state
    
    // Tap tempo state
    uint32_t lastTapTime_ = 0;
    uint32_t tapInterval_ = 0;
    static constexpr uint32_t TAP_TIMEOUT_MS = 2000;  // Reset if no tap within 2 seconds
};

} // namespace perspective

#endif // PERSPECTIVE_PERSPECTIVE_H