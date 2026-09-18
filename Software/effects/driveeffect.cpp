#include "driveeffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

// Butterworth Q values for a 4th-order lowpass built from two biquads.
static constexpr float kButterQ1 = 0.54119610f;
static constexpr float kButterQ2 = 1.30656296f;

// Anti-aliasing cutoff as a fraction of the base (pre-oversampling) rate.
static constexpr float kCutoffFraction = 0.45f;

// Inter-stage scoop bandwidth. Wide enough to hollow the mids without
// sounding like a notch.
static constexpr float kScoopQ = 0.70f;

static const char* kStageNames[] = {"2", "3"};

} // namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

DriveEffect::DriveEffect()
    : Effect("Drive")
    , stageCount_(3)
    , level_lin_(1.0f)
    , scoop_db_(-9.0f)
    , scoop_freq_(650.0f)
    , designedStages_(0)
    , dcPrevIn_(0.0f)
    , dcPrevOut_(0.0f)
    , envelope_(0.0f) {
    for (size_t i = 0; i < kMaxStages; ++i) {
        stageGain_[i] = 1.0f;
        bias_[i] = 0.0f;
        biasOffset_[i] = 0.0f;
    }
}

DriveEffect::~DriveEffect() {
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void DriveEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // K2: Gain — total pre-gain into the cascade, split across the stages
    AddParameter(new PotentiometerParameter("K2 Gain", 0.0f, 48.0f, 32.0f, PotCurve::LIN, MACRO_KNOB_DEPTH_IDX));
    parameters_.back()->SetMacroRole(MacroRole::DEPTH);

    // Stages — 2 is looser and more open, 3 is tighter and more compressed
    PotentiometerParameter* stagesParam =
        new PotentiometerParameter("Stages", 0.0f, 1.0f, 1.0f, PotCurve::LIN, -1);
    stagesParam->SetDisplayType(DisplayType::DISCRETE);
    stagesParam->SetDiscreteValues(kStageNames, 2);
    AddParameter(stagesParam);

    // Asymmetry — DC offset into each shaper; drives the even harmonics
    AddParameter(new PotentiometerParameter("Asym", 0.0f, 1.0f, 0.45f, PotCurve::LIN, -1));

    // Scoop — depth of the mid dip ahead of each stage
    AddParameter(new PotentiometerParameter("Scoop", 0.0f, 18.0f, 9.0f, PotCurve::LIN, -1));

    // Scoop Freq — where the dip sits
    AddParameter(new PotentiometerParameter("Scoop Hz", 300.0f, 1200.0f, 650.0f, PotCurve::LOG, -1));

    // Level — output trim, compensates for the gain the cascade adds
    AddParameter(new PotentiometerParameter("Level", -24.0f, 12.0f, -6.0f, PotCurve::LIN, -1));

    DesignFilters();
    Update();
}

// ---------------------------------------------------------------------------
// Filter design
// ---------------------------------------------------------------------------

void DriveEffect::DesignFilters() {
    const float osRate = sampleRate_ * kOversample;
    const float cutoff = sampleRate_ * kCutoffFraction;

    upFilterA_.SetLowpass(cutoff, kButterQ1, osRate);
    upFilterB_.SetLowpass(cutoff, kButterQ2, osRate);
    downFilterA_.SetLowpass(cutoff, kButterQ1, osRate);
    downFilterB_.SetLowpass(cutoff, kButterQ2, osRate);
}

// The scoop sits ahead of every stage, so its depth compounds down the
// cascade. Divide the requested depth by the stage count to keep "Scoop" a
// total figure — otherwise 10 dB across three stages is a 30 dB hole, and
// switching stage count would lurch the tone rather than just its character.
void DriveEffect::DesignScoop() {
    const float osRate = sampleRate_ * kOversample;
    const float perStage = scoop_db_ / static_cast<float>(stageCount_);

    for (size_t i = 0; i < kMaxStages; ++i) {
        scoop_[i].SetPeaking(scoop_freq_, kScoopQ, -perStage, osRate);
    }

    designedStages_ = stageCount_;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void DriveEffect::Update() {
    const float gain_db = parameters_[kParamGain]->GetValue();
    stageCount_ = parameters_[kParamStages]->GetValue() >= 0.5f ? 3 : 2;

    const float asym = clamp(parameters_[kParamAsymmetry]->GetValue(), 0.0f, 1.0f);
    const float newScoopDb = parameters_[kParamScoop]->GetValue();
    const float newScoopFreq = parameters_[kParamScoopFreq]->GetValue();

    // Only redesign the scoop when something it depends on actually moved —
    // SetPeaking is not cheap.
    if (std::fabsf(newScoopDb - scoop_db_) > 0.01f ||
        std::fabsf(newScoopFreq - scoop_freq_) > 0.5f ||
        stageCount_ != designedStages_) {
        scoop_db_ = newScoopDb;
        scoop_freq_ = newScoopFreq;
        DesignScoop();
    }

    // Spread the total gain evenly across the stages in dB, so switching
    // between 2 and 3 stages changes the character rather than the loudness.
    const float perStageDb = gain_db / static_cast<float>(stageCount_);
    const float perStageLin = std::powf(10.0f, perStageDb / 20.0f);

    for (size_t i = 0; i < kMaxStages; ++i) {
        stageGain_[i] = perStageLin;

        // Later stages are already saturated, so taper the bias — full
        // asymmetry on every stage just sounds lopsided and farty.
        const float taper = 1.0f / (1.0f + static_cast<float>(i));
        bias_[i] = asym * 0.6f * taper;
        biasOffset_[i] = SoftClip(bias_[i]);
    }

    level_lin_ = std::powf(10.0f, parameters_[kParamLevel]->GetValue() / 20.0f);
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void DriveEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        const float x = in[i];

        // --- 2x upsample: zero-stuff, then lowpass. The factor of 2
        // compensates for the energy lost to the inserted zero.
        float os[2];
        os[0] = upFilterB_.Process(upFilterA_.Process(x * 2.0f));
        os[1] = upFilterB_.Process(upFilterA_.Process(0.0f));

        // --- Clipping cascade, at 2x
        for (size_t n = 0; n < 2; ++n) {
            float v = os[n];
            for (size_t s = 0; s < stageCount_; ++s) {
                v = scoop_[s].Process(v);
                v = Stage(v * stageGain_[s], s);
            }
            os[n] = v;
        }

        // --- 2x decimate: both samples must go through the filter to advance
        // its state, but only the second is kept.
        downFilterB_.Process(downFilterA_.Process(os[0]));
        const float y = downFilterB_.Process(downFilterA_.Process(os[1]));

        // --- DC blocker; the asymmetric stages leave a small offset behind
        const float blocked = y - dcPrevIn_ + 0.9995f * dcPrevOut_;
        dcPrevIn_ = y;
        dcPrevOut_ = blocked;

        out[i] = blocked * level_lin_;

        // Envelope for the LED, tracked post-drive
        const float mag = std::fabsf(out[i]);
        if (mag > envelope_) {
            envelope_ += (mag - envelope_) * 0.05f;
        } else {
            envelope_ += (mag - envelope_) * 0.0008f;
        }
    }
}

// ---------------------------------------------------------------------------
// LED feedback
// ---------------------------------------------------------------------------

float DriveEffect::GetEnvelopeBrightness() const {
    return clamp(envelope_ * 1.6f, 0.0f, 1.0f);
}
