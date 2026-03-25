#include "encoder.h"

using namespace daisy;
using namespace perspective;

void Encoder::Init(Pin a, Pin b, Pin click, float update_rate)
{
    last_update_  = System::GetNow();
    update_rate_  = update_rate;

    // Init GPIO for A, and B
    hw_a_.Init(a, GPIO::Mode::INPUT, GPIO::Pull::NOPULL);
    hw_b_.Init(b, GPIO::Mode::INPUT, GPIO::Pull::NOPULL);
    
    // Default Initialization for Switch
    sw_.Init(click, update_rate);
    
    // Set initial states, etc.
    inc_ = 0;
    a_ = b_ = 0xff;
}

void Encoder::Process()
{
    // update no faster than 1kHz
    uint32_t now = System::GetNow();

    if(now - last_update_ >= 1)
    {
        last_update_ = now;

        // Read raw hardware states and shift into debounced state
        a_ = (a_ << 1) | hw_a_.Read();
        b_ = (b_ << 1) | hw_b_.Read();

        // infer increment direction
        inc_ = 0;
        if((a_ & 0x03) == 0x02 && (b_ & 0x03) == 0x00)
        {
            inc_ = 1;
        }
        else if((b_ & 0x03) == 0x02 && (a_ & 0x03) == 0x00)
        {
            inc_ = -1;
        }
    }
}