#include "switch.h"

using namespace daisy;
using namespace perspective;

void Switch::Init(Pin        pin,
                  float      update_rate,
                  Type       t,
                  Polarity   pol,
                  GPIO::Pull pu)
{
    last_update_  = System::GetUs();
    update_rate_  = update_rate;
    state_        = 0x00;
    t_            = t;
    // Flip may seem opposite to logical direction,
    // but here 1 is pressed, 0 is not.
    flip_ = pol == POLARITY_INVERTED ? true : false;
    hw_gpio_.Init(pin, GPIO::Mode::INPUT, pu);
}

void Switch::Init(Pin pin, float update_rate)
{
    Init(pin,
         update_rate,
         TYPE_MOMENTARY,
         POLARITY_INVERTED,
         GPIO::Pull::NOPULL);
}

void Switch::Process()
{
    // Read the raw hardware state and shift into debounced state
    const bool new_val = hw_gpio_.Read();
    state_ = (state_ << 1) | (flip_ ? !new_val : new_val);
    
    // Set time at which button was pressed
    if(state_ == 0x7f)
        rising_edge_time_ = System::GetNow();
}
