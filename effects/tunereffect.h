#ifndef PERSPECTIVE_TUNEREFFECT_H
#define PERSPECTIVE_TUNEREFFECT_H

#include "../effect.h"
#include <daisysp.h>
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/signal_conditioner.hpp>
#include <q/support/literals.hpp>
#include <cmath>

using namespace daisysp;
using namespace cycfi::q::literals;

namespace perspective {

// Note names for display
static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

class TunerEffect : public Effect {
public:
    TunerEffect();
    ~TunerEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;

    // Tuner-specific methods
    float GetDetectedFrequency() const { return detectedFrequency_; }
    float GetCentsOffset() const { return centsOffset_; }
    const char* GetNoteName() const { return noteName_; }
    int GetNoteOctave() const { return noteOctave_; }
    bool IsSignalDetected() const { return signalDetected_; }
    float GetSignalLevel() const { return signalLevel_; }

private:
    // Pitch detection using cycfi/q
    void DetectPitch(float sample);
    void UpdateNoteInfo();
    float FrequencyToCents(float frequency, float targetFrequency);
    
    // Tuning reference (A4 = 440 Hz by default)
    float tuningReference_;
    
    // Detection state
    float detectedFrequency_;
    float centsOffset_;
    const char* noteName_;
    int noteOctave_;
    bool signalDetected_;
    float signalLevel_;
    
    // Cycfi/q components
    cycfi::q::pitch_detector* pitchDetector_;
    cycfi::q::signal_conditioner* signalConditioner_;
    
    // Frequency range
    static constexpr float MIN_FREQUENCY = 40.0f;   // Low E on bass
    static constexpr float MAX_FREQUENCY = 1500.0f; // High notes
};

} // namespace perspective

#endif // PERSPECTIVE_TUNEREFFECT_H
