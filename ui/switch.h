#ifndef SWITCH_H
#define SWITCH_H

#include "daisy_core.h"
#include "per/gpio.h"
#include "sys/system.h"

using namespace daisy;

namespace perspective {

/**
 * @brief Hardware Interface for momentary/latching switches
 * Similar to daisy::Switch but with debouncing logic separated for event handling
 * @author Generated for Perspective
 * @date January 2026
 */
class Switch
{
  public:
    /** Specifies the expected behavior of the switch */
    enum Type
    {
        TYPE_TOGGLE,    /**< Toggle switch */
        TYPE_MOMENTARY, /**< Momentary switch */
    };

    /** Specifies whether the pressed state is HIGH or LOW */
    enum Polarity
    {
        POLARITY_NORMAL,   /**< Pressed = HIGH */
        POLARITY_INVERTED, /**< Pressed = LOW */
    };

    Switch() {}
    ~Switch() {}

    /**
     * Initializes the switch object with a given port/pin combo
     * @param pin Port/pin object to tell the switch which hardware pin to use
     * @param update_rate Update rate in Hz (for time calculations)
     * @param t Switch type -- Default: TYPE_MOMENTARY
     * @param pol Switch polarity -- Default: POLARITY_INVERTED
     * @param pu Switch pull up/down -- Default: NOPULL
     */
    void Init(Pin        pin,
              float      update_rate,
              Type       t,
              Polarity   pol,
              GPIO::Pull pu = GPIO::Pull::NOPULL);

    /**
     * Simplified Init
     * @param pin Port/pin object to tell the switch which hardware pin to use
     * @param update_rate Update rate in Hz (for time calculations)
     */
    void Init(Pin pin, float update_rate = 1000.f);

    /**
     * Reads and debounces the current hardware state
     * Returns true if state changed
     */
    void Process();

    /** @return true if a button was just pressed */
    inline bool RisingEdge() const { return state_ == 0x7f; }

    /** @return true if the button was just released */
    inline bool FallingEdge() const { return state_ == 0x80; }

    /** @return true if the button is held down (or if the toggle is on) */
    inline bool Pressed() const { return state_ == 0xff; }

    /** @return true if the button is held down, without debouncing */
    inline bool RawState()
    {
        const bool raw = hw_gpio_.Read();
        return flip_ ? !raw : raw;
    }

    inline uint8_t DebouncedState() const { return state_; }

    /** @return the time in microseconds that the button has been held (or toggle has been on) */
    inline int TimeHeld() const
    {
        return Pressed() ? System::GetNow() - rising_edge_time_ : 0;
    }

    /**
     * Set update rate for time calculations
     * @param update_rate Update rate in Hz
     */
    inline void SetUpdateRate(float update_rate) { update_rate_ = update_rate; }

  protected:
    uint32_t last_update_;
    float    update_rate_;
    Type     t_;
    GPIO     hw_gpio_;
    uint8_t  state_;
    bool     flip_;
    uint32_t rising_edge_time_;
};

} // namespace perspective

#endif // SWITCH_H
