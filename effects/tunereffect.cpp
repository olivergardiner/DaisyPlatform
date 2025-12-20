#include "tunereffect.h"
#include "../controls.h"
#include <cmath>

using namespace perspective;
using namespace daisysp;

TunerEffect::TunerEffect()
    : Effect("Tuner")
    , tuningReference_(440.0f)
    , detectedFrequency_(0.0f)
    , centsOffset_(0.0f)
    , noteName_("--")
    , noteOctave_(0)
    , signalDetected_(false)
    , signalLevel_(0.0f)
    , pitchDetector_(nullptr)
    , signalConditioner_(nullptr) {
}

TunerEffect::~TunerEffect() {
    delete pitchDetector_;
    delete signalConditioner_;
}

void TunerEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    
    // Add parameter: Tuning Reference (default A4 = 440Hz, range 430-450Hz)
    AddParameter(new PotentiometerParameter("Reference", 430.0f, 450.0f, 440.0f, PotCurve::LIN, KNOB_1_IDX));
    
    // Initialize cycfi/q pitch detector
    // Constructor: pitch_detector(lowest_freq, highest_freq, sps, hysteresis)
    cycfi::q::frequency lowest = MIN_FREQUENCY * 1_Hz;
    cycfi::q::frequency highest = MAX_FREQUENCY * 1_Hz;
    pitchDetector_ = new cycfi::q::pitch_detector(lowest, highest, sampleRate, -45_dB);
    
    // Initialize signal conditioner with default config
    auto sc_conf = cycfi::q::signal_conditioner::config{};
    signalConditioner_ = new cycfi::q::signal_conditioner(sc_conf, lowest, highest, sampleRate);
    
    // Reset detection state
    detectedFrequency_ = 0.0f;
    centsOffset_ = 0.0f;
    noteName_ = "--";
    noteOctave_ = 0;
    signalDetected_ = false;
    signalLevel_ = 0.0f;
    
    Update();
}

void TunerEffect::Process(float* in, float* out, size_t size) {
    // Tuner passes through the input signal unchanged
    // But analyzes the signal for pitch detection
    
    // Process each sample through signal conditioner and pitch detector
    for (size_t i = 0; i < size; i++) {
        out[i] = in[i];  // Pass through
        DetectPitch(in[i]);
    }
}

void TunerEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    // For stereo, analyze the left channel and pass through both
    
    // Process each sample
    for (size_t i = 0; i < size; i++) {
        outL[i] = inL[i];  // Pass through
        outR[i] = inR[i];  // Pass through
        DetectPitch(inL[i]);  // Analyze left channel
    }
}

void TunerEffect::Update() {
    // Update tuning reference from parameter
    if (parameters_.size() > 0) {
        tuningReference_ = parameters_[0]->GetValue();
    }
}

void TunerEffect::DetectPitch(float sample) {
    if (!pitchDetector_ || !signalConditioner_) {
        return;
    }
    
    // Condition the signal
    float conditioned = (*signalConditioner_)(sample);
    
    // Update signal level from conditioner
    signalLevel_ = signalConditioner_->signal_env();
    
    // Check gate state
    signalDetected_ = signalConditioner_->gate();
    
    // Feed sample to pitch detector
    bool is_ready = (*pitchDetector_)(conditioned);
    
    // Get frequency when ready
    if (is_ready && signalDetected_) {
        float frequency = pitchDetector_->get_frequency();
        if (frequency > 0.0f) {
            detectedFrequency_ = frequency;
            UpdateNoteInfo();
        }
    } else if (!signalDetected_) {
        // No signal, reset
        detectedFrequency_ = 0.0f;
        centsOffset_ = 0.0f;
        noteName_ = "--";
        noteOctave_ = 0;
    }
}

void TunerEffect::UpdateNoteInfo() {
    if (detectedFrequency_ <= 0.0f) {
        noteName_ = "--";
        noteOctave_ = 0;
        centsOffset_ = 0.0f;
        return;
    }
    
    // Calculate MIDI note number from frequency
    // MIDI note = 12 * log2(f / 440) + 69
    float midiNote = 12.0f * log2f(detectedFrequency_ / tuningReference_) + 69.0f;
    
    // Round to nearest note
    int nearestNote = static_cast<int>(roundf(midiNote));
    
    // Calculate cents offset from nearest note
    centsOffset_ = 100.0f * (midiNote - nearestNote);
    
    // Get note name and octave
    int noteIndex = nearestNote % 12;
    noteOctave_ = (nearestNote / 12) - 1;
    noteName_ = NOTE_NAMES[noteIndex];
}

float TunerEffect::FrequencyToCents(float frequency, float targetFrequency) {
    if (frequency <= 0.0f || targetFrequency <= 0.0f) {
        return 0.0f;
    }
    return 1200.0f * log2f(frequency / targetFrequency);
}
