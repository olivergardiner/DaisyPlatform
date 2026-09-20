#include "driveeffect.h"

#include "../controls.h"
#include "../parameters/valueparameter.h"
#include "../parameters/enumparameter.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

// Butterworth pole Qs for an 8th-order lowpass built from four cascaded
// biquads (DriveEffect::kFilterSections of them).
//
// Why 8th order at this cutoff, not the 4th-order pair this used to be: at 2x
// oversampling the fold frequency is the base Nyquist — 24 kHz at the usual
// 48 kHz rate — and with the previous cutoff (0.45 * 48 kHz = 21.6 kHz) that
// is only 1.11x above cutoff, less than a sixth of an octave of transition
// band. Evaluating the actual digital transfer function, that gave only
// -6.6 dB of attenuation at the fold frequency — nowhere near enough once a
// high cascade gain pushes real energy up past 14 kHz: that energy folds back
// into the band as inharmonic aliasing rather than clean harmonic buzz, and it
// gets worse as gain goes up because the nonlinearity is generating more of
// it. With the cutoff lowered to kCutoffFraction and this order, the same
// calculation gives -46.8 dB at the fold frequency — a 40 dB improvement —
// while the passband is essentially untouched below 12 kHz (-0.16 dB there),
// for four extra biquad evaluations per sample that a Cortex-M7 does not
// notice.
static constexpr float kButterQ[4] = {
    0.50979f, 0.60134f, 0.90000f, 2.56291f
};

// Anti-aliasing cutoff as a fraction of the base (pre-oversampling) rate.
// Content above this starts rolling off; everything downstream (tone stack,
// cab) rolls off well before it anyway, so nothing audible is lost by cutting
// here rather than right at the old 21.6 kHz.
static constexpr float kCutoffFraction = 0.30f;

// Inter-stage scoop bandwidth. Wide enough to hollow the mids without
// sounding like a notch.
static constexpr float kScoopQ = 0.70f;

static const char* kStageNames[] = {"2", "3", "4"};

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
    auto* gainParam = new ValueParameter("K2 Gain", 0.0f, 48.0f, 32.0f);
    gainParam->BindPotentiometer(MACRO_KNOB_DEPTH_IDX, PotCurve::LIN);
    gainParam->SetMacroRole(MacroRole::DEPTH);
    AddParameter(gainParam);

    // Stages — 2 is loosest and most open, 4 is tightest and most compressed.
    // Default stays at 3 (index 1) rather than moving to the new max, so
    // existing tunings (Sandman's own Stages macro among them) don't change.
    AddParameter(new EnumParameter("Stages", kStageNames, 3, 1));

    // Asymmetry — DC offset into each shaper; drives the even harmonics
    AddParameter(new ValueParameter("Asym", 0.0f, 1.0f, 0.45f));

    // Scoop — depth of the mid dip ahead of each stage
    AddParameter(new ValueParameter("Scoop", 0.0f, 18.0f, 9.0f));

    // Scoop Freq — where the dip sits
    AddParameter(new ValueParameter("Scoop Hz", 300.0f, 1200.0f, 650.0f));

    // Level — output trim, compensates for the gain the cascade adds
    AddParameter(new ValueParameter("Level", -24.0f, 12.0f, -6.0f));

    DesignFilters();
    Update();
}

// ---------------------------------------------------------------------------
// Filter design
// ---------------------------------------------------------------------------

void DriveEffect::DesignFilters() {
    const float osRate = sampleRate_ * kOversample;
    const float cutoff = sampleRate_ * kCutoffFraction;

    for (Channel& c : channels_) {
        for (size_t f = 0; f < kFilterSections; ++f) {
            c.upFilter[f].SetLowpass(cutoff, kButterQ[f], osRate);
            c.downFilter[f].SetLowpass(cutoff, kButterQ[f], osRate);
        }
    }
}

// The scoop sits ahead of every stage, so its depth compounds down the
// cascade. Divide the requested depth by the stage count to keep "Scoop" a
// total figure — otherwise 10 dB across three stages is a 30 dB hole, and
// switching stage count would lurch the tone rather than just its character.
void DriveEffect::DesignScoop() {
    const float osRate = sampleRate_ * kOversample;
    const float perStage = scoop_db_ / static_cast<float>(stageCount_);

    for (Channel& c : channels_) {
        for (size_t i = 0; i < kMaxStages; ++i) {
            c.scoop[i].SetPeaking(scoop_freq_, kScoopQ, -perStage, osRate);
        }
    }

    designedStages_ = stageCount_;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void DriveEffect::Update() {
    const float gain_db = parameters_[kParamGain]->GetValue();
    // Stages has one option per discrete choice ("2", "3", "4", ...), so the
    // stage count is just that option's index plus 2.
    stageCount_ = 2 + static_cast<size_t>(static_cast<EnumParameter*>(parameters_[kParamStages])->GetSelectedIndex());

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

void DriveEffect::ProcessChannel(Channel& channel, const float* in, float* out,
                                 size_t size, bool trackEnvelope) {
    for (size_t i = 0; i < size; ++i) {
        const float x = in[i];

        // --- 2x upsample: zero-stuff, then lowpass. The factor of 2
        // compensates for the energy lost to the inserted zero.
        float os[2] = {x * 2.0f, 0.0f};
        for (float& s : os) {
            for (size_t f = 0; f < kFilterSections; ++f) {
                s = channel.upFilter[f].Process(s);
            }
        }

        // --- Clipping cascade, at 2x
        for (size_t n = 0; n < 2; ++n) {
            float v = os[n];
            for (size_t s = 0; s < stageCount_; ++s) {
                v = channel.scoop[s].Process(v);
                v = Stage(v * stageGain_[s], s);
            }
            os[n] = v;
        }

        // --- 2x decimate: both samples must go through the filter to advance
        // its state, but only the second is kept.
        for (size_t f = 0; f < kFilterSections; ++f) os[0] = channel.downFilter[f].Process(os[0]);
        for (size_t f = 0; f < kFilterSections; ++f) os[1] = channel.downFilter[f].Process(os[1]);
        const float y = os[1];

        // --- DC blocker; the asymmetric stages leave a small offset behind
        const float blocked = y - channel.dcPrevIn + 0.9995f * channel.dcPrevOut;
        channel.dcPrevIn = y;
        channel.dcPrevOut = blocked;

        out[i] = blocked * level_lin_;

        // Envelope for the LED, tracked post-drive
        if (trackEnvelope) {
            const float mag = std::fabsf(out[i]);
            if (mag > envelope_) {
                envelope_ += (mag - envelope_) * 0.05f;
            } else {
                envelope_ += (mag - envelope_) * 0.0008f;
            }
        }
    }
}

void DriveEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    ProcessChannel(channels_[0], in, out, size, /*trackEnvelope=*/true);
}

void DriveEffect::ProcessStereo(const float* inL, const float* inR,
                                float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    // Independent state per channel. Sharing it would make the filters see an
    // interleaved L/R stream and produce crosstalk rather than two channels.
    ProcessChannel(channels_[0], inL, outL, size, /*trackEnvelope=*/true);
    ProcessChannel(channels_[1], inR, outR, size, /*trackEnvelope=*/false);
}

// ---------------------------------------------------------------------------
// LED feedback
// ---------------------------------------------------------------------------

float DriveEffect::GetEnvelopeBrightness() const {
    return clamp(envelope_ * 1.6f, 0.0f, 1.0f);
}
