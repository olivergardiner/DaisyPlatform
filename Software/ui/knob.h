#pragma once
#ifndef KNOB_H
#define KNOB_H

#include <stdint.h>

namespace perspective {

/**
 * @brief Hardware Interface for control inputs (knobs/potentiometers)
 * Similar to AnalogControl but with conversion logic applied outside Process()
 * This allows for raw value smoothing separate from scaling/offset/flip transformations
 * @author Generated for Perspective
 * @date January 2026
 */
class Knob
{
  public:
    /** Constructor */
    Knob() {}
    
    /** Destructor */
    ~Knob() {}

    /**
     * Initializes the knob control
     * @param adcptr Pointer to the raw ADC read value
     * @param sr Sample rate in Hz that the Process function will be called at
     * @param flip Determines whether the input is flipped (i.e. 1.0 - input)
     * @param invert Determines whether the input is inverted (i.e. -1.0 * input)
     * @param slew_seconds Slew time in seconds for the control to reach a new value
     */
    void Init(uint16_t *adcptr,
              float     sr,
              bool      flip         = false,
              bool      invert       = false,
              float     slew_seconds = 0.002f);

    /**
     * Initializes the Knob for a -5V to 5V inverted input (bipolar CV)
     * @param adcptr Pointer to analog digital converter
     * @param sr Sample rate
     */
    void InitBipolarCv(uint16_t *adcptr, float sr);

    /**
     * Reads and normalizes the raw ADC value (0.0 to 1.0)
     * Returns the raw normalized value without filtering
     * Call this at the rate specified by samplerate at Init time
     */
    inline void Process() {
        raw_ = *adc_;
    }

    /**
     * Applies one-pole smoothing filter to the last value read by Process()
     * Call this from the event handler before dispatching events
     * Returns the smoothed raw value (0.0 to 1.0)
     */
    float Filter();

    /**
     * Returns the current smoothed raw value without reprocessing (0.0 to 1.0)
     */
    inline float RawValue() const { return raw_val_; }

    /**
     * Returns the converted value with flip/invert/scale/offset applied
     */
    inline float Value() const { return ConvertValue(raw_val_); }

    /**
     * Apply conversions to a given raw value (0.0 to 1.0)
     * This applies flip, invert, scale, and offset transformations
     */
    float ConvertValue(float raw_value) const;

    /**
     * Directly set the coefficient of the one-pole smoothing filter
     * @param val Coefficient value (clamped between 0 and 1)
     */
    inline void SetCoeff(float val)
    {
        val = val > 1.f ? 1.f : val;
        val = val < 0.f ? 0.f : val;
        coeff_ = val;
    }

    /**
     * Set the scaling factor for conversion
     * Normally set during initialization, but can be adjusted for calibration
     */
    inline void SetScale(const float scale) { scale_ = scale; }

    /**
     * Set the offset for conversion
     * Normally set during initialization, but can be adjusted for calibration
     */
    inline void SetOffset(const float offset) { offset_ = offset; }

    /**
     * Returns the raw unsigned 16-bit value from the ADC
     */
    inline uint16_t GetRawValue() const { return raw_; }

    /**
     * Returns a normalized float value representing the current ADC value (0.0 to 1.0)
     */
    inline float GetRawFloat() const { return (float)(raw_) / 65535.f; }

    /**
     * Set a new sample rate after initialization
     * @param sample_rate New update rate in Hz
     */
    void SetSampleRate(float sample_rate);

  protected:
    uint16_t *adc_;              // Pointer to raw ADC value
    int       raw_;              // Last raw ADC value
    float     coeff_;            // One-pole filter coefficient
    float     samplerate_;       // Sample rate in Hz
    float     raw_val_;          // Smoothed raw value (0.0 to 1.0)
    float     scale_;            // Scaling factor
    float     offset_;           // Offset value
    bool      flip_;             // Flip the input
    bool      invert_;           // Invert the input
    bool      is_bipolar_;       // Bipolar CV mode
    float     slew_seconds_;     // Slew time in seconds
};

} // namespace perspective

#endif // KNOB_H
