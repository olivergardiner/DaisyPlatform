#ifndef PERSPECTIVE_NOPULLCONTROLS_H
#define PERSPECTIVE_NOPULLCONTROLS_H

#include "daisy_seed.h"
#include "hid/encoder.h"
#include "per/gpio.h"

using namespace daisy;

namespace perspective {

// Custom Switch subclass that initializes with NOPULL
class NoPullSwitch : public Switch {
public:
    NoPullSwitch();
    
    void Init(Pin pin, 
              float update_rate = 0.f,
              Type type = TYPE_MOMENTARY,
              Polarity polarity = POLARITY_NORMAL);
};

// Custom Encoder subclass that initializes with NOPULL
class NoPullEncoder : public Encoder {
public:
    NoPullEncoder() : Encoder() {}
    
    /** Initializes the encoder with the specified hardware pins.
     * All pins use NOPULL configuration
     */
    void Init(Pin a, Pin b, Pin click = Pin(), float update_rate = 0.f);
};

} // namespace perspective

#endif // PERSPECTIVE_NOPULLCONTROLS_H
