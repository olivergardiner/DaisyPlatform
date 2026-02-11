#include "knob.h"
#include <math.h>

using namespace perspective;

void Knob::Init(uint16_t *adcptr,
                float     sr,
                bool      flip,
                bool      invert,
                float     slew_seconds)
{
    raw_val_      = 0.0f;
    adc_          = adcptr;
    samplerate_   = sr;
    SetCoeff(1.0f / (slew_seconds * samplerate_ * 0.5f));
    scale_        = 1.0f;
    offset_       = 0.0f;
    flip_         = flip;
    invert_       = invert;
    is_bipolar_   = false;
    slew_seconds_ = slew_seconds;
}

void Knob::InitBipolarCv(uint16_t *adcptr, float sr)
{
    raw_val_    = 0.0f;
    adc_        = adcptr;
    samplerate_ = sr;
    SetCoeff(1.0f / (0.002f * samplerate_ * 0.5f));
    scale_      = 2.0f;
    offset_     = 0.5f;
    flip_       = false;
    invert_     = true;
    is_bipolar_ = true;
}

float Knob::Filter()
{
    // Get the current raw ADC value
    float t = raw_/65535.f;
    
    // Apply one-pole smoothing filter
    raw_val_ += coeff_ * (t - raw_val_);
    
    return raw_val_;
}

float Knob::ConvertValue(float raw_value) const
{
    float t = raw_value;
    
    // Apply flip if enabled
    if(flip_)
        t = 1.f - t;
    
    // Apply offset and scale
    t = (t - offset_) * scale_;
    
    // Apply invert if enabled
    if(invert_)
        t = -t;
    
    return t;
}

void Knob::SetSampleRate(float sample_rate)
{
    samplerate_ = sample_rate;
    float slew  = is_bipolar_ ? .002f : slew_seconds_;
    SetCoeff(1.0f / (slew * samplerate_ * 0.5f));
}
