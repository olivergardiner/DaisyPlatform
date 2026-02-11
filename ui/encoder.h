#pragma once
#ifndef ENCODER_H
#define ENCODER_H

#include "daisy_core.h"
#include "per/gpio.h"
#include "switch.h"

using namespace daisy;

namespace perspective {

/**
 * @brief Hardware Interface for Quadrature Encoders
 * Similar to daisy::Encoder but with debouncing logic separated for event handling
 * @author Generated for Perspective
 * @date January 2026
 */
class Encoder
{
  public:
    Encoder() {}
    ~Encoder() {}

    /**
     * Initializes the encoder with the specified hardware pins
     * @param a Pin for encoder A channel
     * @param b Pin for encoder B channel
     * @param click Pin for encoder button/click
     * @param update_rate Update rate in Hz (for time calculations)
     */
    void Init(Pin a, Pin b, Pin click, float update_rate = 1000.f);

    /**
     * Reads and debounces the current hardware state
     * Returns true if state changed
     */
    void Process();

    /**
     * Returns +1 if the encoder was turned clockwise, -1 if counter-clockwise, or 0 if not turned
     */
    inline int32_t Increment() const { return inc_; }

    /** Returns true if the encoder was just pressed */
    inline bool RisingEdge() const { return sw_.RisingEdge(); }

    /** Returns true if the encoder was just released */
    inline bool FallingEdge() const { return sw_.FallingEdge(); }

    /** Returns true while the encoder is held down */
    inline bool Pressed() const { return sw_.Pressed(); }

    /** Returns the time in milliseconds that the encoder has been held down */
    inline int TimeHeld() const { return sw_.TimeHeld(); }

    /** Returns a pointer to the encoder's switch */
    inline Switch* GetSwitch() { return &sw_; }

    /**
     * Set update rate for time calculations
     * @param update_rate Update rate in Hz
     */
    inline void SetUpdateRate(float update_rate) 
    { 
        update_rate_ = update_rate;
        sw_.SetUpdateRate(update_rate);
    }

  private:
    uint32_t last_update_;
    float    update_rate_;
    Switch   sw_;
    GPIO     hw_a_, hw_b_;
    uint8_t  a_, b_;
    int32_t  inc_;
};

} // namespace perspective

#endif // ENCODER_H
