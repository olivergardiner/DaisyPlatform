#include "tunereffect.h"
#include "../controls.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

TunerEffect::TunerEffect()
    : Effect("Tuner")
    , tuningReference_(440.0f)
    , detectedFrequency_(0.0f)
    , centsOffset_(0.0f)
    , noteName_("--")
    , noteOctave_(0)
    , signalDetected_(false)
    , signalLevel_(0.0f)
    , yinWritePos_(0)
    , yinEnergyAccum_(0.0f)
    , noteHoldFrames_(0) {
    for (int i = 0; i < YIN_BUFFER_SIZE; i++) yinBuffer_[i] = 0.0f;
}

TunerEffect::~TunerEffect() {
}

void TunerEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    yinWritePos_    = 0;
    yinEnergyAccum_ = 0.0f;
    detectedFrequency_ = 0.0f;
    centsOffset_       = 0.0f;
    noteName_          = "--";
    noteOctave_        = 0;
    signalDetected_    = false;
    signalLevel_       = 0.0f;
    noteHoldFrames_    = 0;

    if (parameters_.empty()) {
        AddParameter(new EncoderParameter("Tuner Ref", 420.0f, 460.0f, 440.0f, 0.5f, ENCODER_1_IDX, 0));
    }

    Update();
}

void TunerEffect::Process(float* in, float* out, size_t size) {
    for (size_t i = 0; i < size; i++) {
        out[i] = in[i];
        ProcessSample(in[i]);
    }
}

void TunerEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    for (size_t i = 0; i < size; i++) {
        outL[i] = inL[i];
        outR[i] = inR[i];
        ProcessSample(inL[i]);
    }
}

void TunerEffect::Update() {
    tuningReference_ = parameters_.size() > kParamTuningReference
        ? parameters_[kParamTuningReference]->GetValue()
        : 440.0f;
}

void TunerEffect::ProcessSample(float sample) {
    yinBuffer_[yinWritePos_] = sample;
    yinEnergyAccum_ += sample * sample;
    yinWritePos_++;

    if (yinWritePos_ >= YIN_BUFFER_SIZE) {
        float rms = sqrtf(yinEnergyAccum_ / YIN_BUFFER_SIZE);
        signalLevel_ = rms;

        if (rms >= SIGNAL_RMS_THRESHOLD) {
            signalDetected_ = true;
            RunYIN();
        } else {
            if(noteHoldFrames_ > 0) {
                noteHoldFrames_--;
            } else {
                signalDetected_    = false;
                detectedFrequency_ = 0.0f;
                centsOffset_       = 0.0f;
                noteName_          = "-";
                noteOctave_        = 0;
            }
        }

        yinWritePos_    = 0;
        yinEnergyAccum_ = 0.0f;
    }
}

void TunerEffect::RunYIN() {
    // YIN algorithm (de Cheveigne & Kawahara, 2002)
    // Step 1+2: Difference function + cumulative mean normalised difference (CMNDF)
    yinDiff_[0]        = 1.0f;
    float runningSum   = 0.0f;

    for (int tau = 1; tau < YIN_HALF_SIZE; tau++) {
        float diff = 0.0f;
        for (int j = 0; j < YIN_HALF_SIZE; j++) {
            float delta = yinBuffer_[j] - yinBuffer_[j + tau];
            diff += delta * delta;
        }
        runningSum += diff;
        yinDiff_[tau] = (runningSum > 0.0f) ? diff * tau / runningSum : 0.0f;
    }

    // Step 3: Absolute threshold — first local minimum below threshold
    int tau = 2;
    while (tau < YIN_HALF_SIZE - 1) {
        if (yinDiff_[tau] < YIN_THRESHOLD) {
            while (tau + 1 < YIN_HALF_SIZE - 1 && yinDiff_[tau + 1] < yinDiff_[tau]) {
                tau++;
            }
            break;
        }
        tau++;
    }

    if (tau >= YIN_HALF_SIZE - 1) {
        if(noteHoldFrames_ > 0) {
            noteHoldFrames_--;
        }
        return; // No reliable pitch found; keep last valid reading
    }

    // Step 4: Parabolic interpolation for sub-sample accuracy
    float betterTau;
    {
        float s0    = yinDiff_[tau - 1];
        float s1    = yinDiff_[tau];
        float s2    = yinDiff_[tau + 1];
        float denom = 2.0f * s1 - s0 - s2;
        betterTau   = (denom != 0.0f) ? tau + (s2 - s0) / (2.0f * denom) : (float)tau;
    }

    float frequency = sampleRate_ / betterTau;
    if (frequency >= MIN_FREQUENCY && frequency <= MAX_FREQUENCY) {
        detectedFrequency_ = (detectedFrequency_ > 0.0f)
            ? detectedFrequency_ + FREQUENCY_SMOOTHING * (frequency - detectedFrequency_)
            : frequency;
        noteHoldFrames_ = NOTE_HOLD_MAX_FRAMES;
        UpdateNoteInfo();
    }
}

void TunerEffect::UpdateNoteInfo() {
    if (detectedFrequency_ <= 0.0f) {
        noteName_    = "-";
        noteOctave_  = 0;
        centsOffset_ = 0.0f;
        return;
    }

    float midiNote   = 12.0f * log2f(detectedFrequency_ / tuningReference_) + 69.0f;
    int nearestNote  = static_cast<int>(roundf(midiNote));
    float targetCents = 100.0f * (midiNote - nearestNote);
    centsOffset_ = centsOffset_ + CENTS_SMOOTHING * (targetCents - centsOffset_);

    if (nearestNote < 0) nearestNote = 0;
    noteOctave_ = (nearestNote / 12) - 1;
    noteName_   = NOTE_NAMES[nearestNote % 12];
}

float TunerEffect::FrequencyToCents(float frequency, float targetFrequency) {
    if (frequency <= 0.0f || targetFrequency <= 0.0f) {
        return 0.0f;
    }
    return 1200.0f * log2f(frequency / targetFrequency);
}
