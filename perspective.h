#pragma once

#include "hardware.h"
#include "ui/ui.h"

namespace perspective {

class Effect;  // Forward declaration

class Perspective : public UI {
public:
    Perspective();
    ~Perspective() override;
    
    void Init() override;
    
private:
    void RegisterEventListeners();
    void toggleBypass();
    void HandleTapTempo();
    
    Hardware hardware;
    Effect* currentEffect_;

    bool bypassMode_ = true;
    
    // Tap tempo state
    uint32_t lastTapTime_ = 0;
    uint32_t tapInterval_ = 0;
    static constexpr uint32_t TAP_TIMEOUT_MS = 2000;  // Reset if no tap within 2 seconds
};

} // namespace perspective
