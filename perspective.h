#ifndef PERSPECTIVE_PERSPECTIVE_H
#define PERSPECTIVE_PERSPECTIVE_H

#include "hardware.h"
#include "ui/ui.h"

#include <vector>

namespace perspective {

class Effect;  // Forward declaration

class Perspective : public UI {
public:
    Perspective();
    ~Perspective() override;
    
    void Init() override;
    void Exec() override;
    void LightLed(bool on);
    
    void AudioCallbackImpl(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);
    
protected:
    void RegisterEventListeners();
    void LoadEffects();
    void toggleBypass();
    void HandleTapTempo();
    
    Hardware hardware;
    Effect* currentEffect_;
    std::vector<Effect*> effects_;

    bool bypassMode_ = true;
    
    // Tap tempo state
    uint32_t lastTapTime_ = 0;
    uint32_t tapInterval_ = 0;
    static constexpr uint32_t TAP_TIMEOUT_MS = 2000;  // Reset if no tap within 2 seconds
};

} // namespace perspective

#endif // PERSPECTIVE_PERSPECTIVE_H