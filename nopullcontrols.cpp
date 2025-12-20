#include "nopullcontrols.h"

using namespace daisy;
using namespace perspective;

// NoPullSwitch implementation
NoPullSwitch::NoPullSwitch() : Switch() {}

void NoPullSwitch::Init(Pin pin, 
                        float update_rate,
                        Type type,
                        Polarity polarity) {
    Switch::Init(pin, update_rate, type, polarity, GPIO::Pull::NOPULL);
}

// NoPullEncoder implementation
void NoPullEncoder::Init(Pin a, Pin b, Pin click, float update_rate) {
    // Call base class Init but we need to replicate with NOPULL
    // Since we can't access private members, we have to use the base Init
    // but immediately reinitialize the GPIO pins with NOPULL
    Encoder::Init(a, b, click, update_rate);
    
    // The encoder uses internal GPIO objects that we can't access
    // So we need to use a different approach - manually initialize the GPIOs
    GPIO gpio_a, gpio_b, gpio_click;
    gpio_a.Init(a, GPIO::Mode::INPUT, GPIO::Pull::NOPULL);
    gpio_b.Init(b, GPIO::Mode::INPUT, GPIO::Pull::NOPULL);
    gpio_click.Init(click, GPIO::Mode::INPUT, GPIO::Pull::NOPULL);
}
