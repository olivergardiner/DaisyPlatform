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
    steps_per_detent_ = 2;
    state_            = (hw_a_.Read() << 1) | hw_b_.Read();
    sample_candidate_state_ = state_;
    sample_stability_count_ = 0;
    direction_              = 1;
}

void Encoder::Process()
{
    static constexpr int8_t transition_lut[16]
        = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

    sw_.Process();

    inc_ = 0;

    const uint8_t raw_state = (hw_a_.Read() << 1) | hw_b_.Read();
    if(raw_state == state_)
    {
        sample_candidate_state_ = state_;
        sample_stability_count_ = 0;
        return;
    }

    if(raw_state != sample_candidate_state_)
    {
        sample_candidate_state_ = raw_state;
        sample_stability_count_ = 1;
        return;
    }

    if(sample_stability_count_ < 1)
    {
        sample_stability_count_++;
        return;
    }

    const uint8_t new_state = raw_state;
    sample_candidate_state_ = new_state;
    sample_stability_count_ = 0;

    const int8_t  delta     = transition_lut[(state_ << 2) | new_state];

    // If a non-adjacent transition appears, resync without emitting movement.
    if(delta == 0)
    {
        state_ = new_state;
        step_accumulator_ = 0;
        return;
    }

    state_ = new_state;
    step_accumulator_ += delta * direction_;

    if(step_accumulator_ >= static_cast<int8_t>(steps_per_detent_))
    {
        inc_ = step_accumulator_ / static_cast<int8_t>(steps_per_detent_);
        step_accumulator_ %= static_cast<int8_t>(steps_per_detent_);
    }
    else if(step_accumulator_ <= -static_cast<int8_t>(steps_per_detent_))
    {
        inc_ = step_accumulator_ / static_cast<int8_t>(steps_per_detent_);
        step_accumulator_ %= static_cast<int8_t>(steps_per_detent_);
    }
}