#ifndef PERSPECTIVE_TUNEREFFECT_H
#define PERSPECTIVE_TUNEREFFECT_H

#include "effect.h"
#include <daisysp.h>
#include <cmath>

using namespace daisysp;

namespace perspective {

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
    float GetTuningReference() const { return tuningReference_; }

private:
    void ProcessSample(float sample);
    void RunYIN();
    void UpdateNoteInfo();
    float FrequencyToCents(float frequency, float targetFrequency);

    enum ParamIndex {
        kParamTuningReference = 0
    };

    float tuningReference_;

    // Detection state
    float detectedFrequency_;
    float centsOffset_;
    const char* noteName_;
    int noteOctave_;
    bool signalDetected_;
    float signalLevel_;

    // YIN pitch detection
    // Buffer covers down to ~23 Hz at 48 kHz (well below low E = 41.2 Hz)
    static constexpr int   YIN_BUFFER_SIZE      = 4096;
    static constexpr int   YIN_HALF_SIZE        = YIN_BUFFER_SIZE / 2;
    static constexpr float YIN_THRESHOLD        = 0.15f;
    static constexpr float SIGNAL_RMS_THRESHOLD = 0.005f;

    float yinBuffer_[YIN_BUFFER_SIZE];
    float yinDiff_[YIN_HALF_SIZE];
    int   yinWritePos_;
    float yinEnergyAccum_;
    int   noteHoldFrames_;

    static constexpr int   NOTE_HOLD_MAX_FRAMES = 3;
    static constexpr float FREQUENCY_SMOOTHING  = 0.35f;
    static constexpr float CENTS_SMOOTHING      = 0.30f;

    // Frequency range
    static constexpr float MIN_FREQUENCY = 40.0f;
    static constexpr float MAX_FREQUENCY = 1500.0f;
};

} // namespace perspective

#endif // PERSPECTIVE_TUNEREFFECT_H
