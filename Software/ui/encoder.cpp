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
    inc_              = 0;
    step_accumulator_ = 0;
    state_            = (hw_a_.Read() << 1) | hw_b_.Read();
}

void Encoder::Process()
{
    static constexpr int8_t transition_lut[16]
        = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

    sw_.Process();

    const uint8_t new_state = (hw_a_.Read() << 1) | hw_b_.Read();
    const int8_t  delta     = transition_lut[(state_ << 2) | new_state];

    inc_ = 0;

    if(delta == 0)
    {
        state_ = new_state;
        return;
    }

    state_ = new_state;
    step_accumulator_ += delta;

    if(step_accumulator_ >= 4)
    {
        inc_ = 1;
        step_accumulator_ = 0;
    }
    else if(step_accumulator_ <= -4)
    {
        inc_ = -1;
        step_accumulator_ = 0;
    }
}