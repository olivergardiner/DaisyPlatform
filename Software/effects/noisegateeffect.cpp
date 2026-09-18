#include "noisegateeffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

// Detector smoothing. Fast enough to catch a pick attack, slow enough not to
// re-trigger on individual cycles of a low E.
static constexpr float kDetectorAttack = 0.35f;
static constexpr float kDetectorRelease = 0.0015f;

static inline float DbToLin(float db) {
    return std::powf(10.0f, db / 20.0f);
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

NoiseGateEffect::NoiseGateEffect()
    : Effect("Noise Gate")
    , openLevel_(DbToLin(-52.0f))
    , closeLevel_(DbToLin(-58.0f))
    , attackCoeff_(0.0f)
    , releaseCoeff_(0.0f)
    , holdSamples_(0)
    , detector_(0.0f)
    , gain_(0.0f)
    , holdCounter_(0)
    , open_(false) {
}

NoiseGateEffect::~NoiseGateEffect() {
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void NoiseGateEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // Threshold — level at which the gate opens
    AddParameter(new PotentiometerParameter("Threshold", -80.0f, -20.0f, -52.0f, PotCurve::LIN, -1));

    // Hysteresis — how far below the open threshold the gate closes again
    AddParameter(new PotentiometerParameter("Hyst", 0.0f, 18.0f, 6.0f, PotCurve::LIN, -1));

    // Attack — how fast the gate opens once triggered
    AddParameter(new PotentiometerParameter("Attack", 0.1f, 20.0f, 1.0f, PotCurve::LOG, -1));

    // Hold — minimum time the gate stays open after falling below threshold
    AddParameter(new PotentiometerParameter("Hold", 0.0f, 250.0f, 40.0f, PotCurve::LIN, -1));

    // Release — how fast the gate closes; short for tight chugs
    AddParameter(new PotentiometerParameter("Release", 5.0f, 500.0f, 80.0f, PotCurve::LOG, -1));

    Update();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void NoiseGateEffect::Update() {
    const float threshold_db = parameters_[kParamThreshold]->GetValue();
    const float hysteresis_db = std::max(0.0f, parameters_[kParamHysteresis]->GetValue());

    openLevel_ = DbToLin(threshold_db);
    closeLevel_ = DbToLin(threshold_db - hysteresis_db);

    const float attack_ms = std::max(0.05f, parameters_[kParamAttack]->GetValue());
    const float release_ms = std::max(1.0f, parameters_[kParamRelease]->GetValue());
    attackCoeff_ = 1.0f - std::expf(-1.0f / (attack_ms * 0.001f * sampleRate_));
    releaseCoeff_ = 1.0f - std::expf(-1.0f / (release_ms * 0.001f * sampleRate_));

    const float hold_ms = std::max(0.0f, parameters_[kParamHold]->GetValue());
    holdSamples_ = static_cast<uint32_t>(hold_ms * 0.001f * sampleRate_);
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void NoiseGateEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    // Key off the pedal's dry input when it has been supplied, so the gate
    // tracks picking dynamics rather than whatever feeds it. Keyed off a
    // post-drive signal instead, the detector would be looking at something
    // compressed to within a few dB of full scale, and the gate would hang
    // open long after the note had stopped.
    const float* key = (keyInput_ && keySize_ == size) ? keyInput_ : in;

    for (size_t i = 0; i < size; ++i) {
        const float mag = std::fabsf(key[i]);

        // Peak-ish detector
        if (mag > detector_) {
            detector_ += (mag - detector_) * kDetectorAttack;
        } else {
            detector_ += (mag - detector_) * kDetectorRelease;
        }

        // Open/close decision with hysteresis and hold
        if (detector_ >= openLevel_) {
            open_ = true;
            holdCounter_ = holdSamples_;
        } else if (detector_ < closeLevel_) {
            if (holdCounter_ > 0) {
                --holdCounter_;
            } else {
                open_ = false;
            }
        }

        // Ramp the gain rather than switching it, or every note starts with a click
        const float target = open_ ? 1.0f : 0.0f;
        const float coeff = (target > gain_) ? attackCoeff_ : releaseCoeff_;
        gain_ += (target - gain_) * coeff;

        out[i] = in[i] * gain_;
    }
}

// ---------------------------------------------------------------------------
// LED feedback — lit while the gate is passing signal
// ---------------------------------------------------------------------------

float NoiseGateEffect::GetEnvelopeBrightness() const {
    return clamp(gain_, 0.0f, 1.0f);
}
