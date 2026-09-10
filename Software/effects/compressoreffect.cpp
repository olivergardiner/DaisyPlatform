#include "compressoreffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kMinLevel = 1e-6f; // -120 dBFS floor

static inline float Clamp01(float x) {
    return std::max(0.0f, std::min(x, 1.0f));
}

} // namespace

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

float CompressorEffect::AttackCoeff(float attack_ms, float sampleRate) {
    return std::expf(-1.0f / (attack_ms * 0.001f * sampleRate));
}

float CompressorEffect::ReleaseCoeff(float release_ms, float sampleRate) {
    return std::expf(-1.0f / (release_ms * 0.001f * sampleRate));
}

// Returns linear gain multiplier (makeup already NOT included — applied separately).
float CompressorEffect::ComputeGain(float level_lin) const {
    if (level_lin < kMinLevel) {
        return 1.0f;
    }

    const float level_db = 20.0f * std::log10f(level_lin);

    if (level_db <= threshold_db_) {
        return 1.0f;
    }

    // Gain reduction in dB: GR = (threshold - level) * (1 - 1/ratio)
    const float gain_reduction_db = (threshold_db_ - level_db) * (1.0f - 1.0f / ratio_);
    return std::powf(10.0f, gain_reduction_db / 20.0f);
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

CompressorEffect::CompressorEffect()
    : Effect("Compressor")
    , threshold_db_(-20.0f)
    , ratio_(4.0f)
    , attack_coeff_(0.0f)
    , release_coeff_(0.0f)
    , makeup_lin_(1.0f)
    , mix_(1.0f)
    , envelope_(0.0f)
    , envelopeL_(0.0f)
    , envelopeR_(0.0f) {
}

CompressorEffect::~CompressorEffect() {
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void CompressorEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    // Threshold — sets the level above which compression starts (-60 to 0 dB)
    AddParameter(new PotentiometerParameter("Threshold", -60.0f, 0.0f, -20.0f, PotCurve::LIN, -1));

    // Ratio — compression ratio (1:1 = bypass, 20:1 ≈ limiting)
    AddParameter(new PotentiometerParameter("Ratio", 1.0f, 20.0f, 4.0f, PotCurve::LOG, -1));

    // Attack — time for the compressor to engage (0.1 ms – 200 ms)
    AddParameter(new PotentiometerParameter("Attack", 0.1f, 200.0f, 10.0f, PotCurve::LOG, -1));

    // Release — time for the compressor to disengage (10 ms – 2000 ms)
    AddParameter(new PotentiometerParameter("Release", 10.0f, 2000.0f, 100.0f, PotCurve::LOG, -1));

    // K5: Makeup Gain — post-compression gain (0 – 30 dB)
    AddParameter(new PotentiometerParameter("Makeup", 0.0f, 30.0f, 6.0f, PotCurve::LIN, -1));

    // Mix — wet/dry blend (0 = dry, 1 = full compression)
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 1.0f, PotCurve::LIN, MACRO_KNOB_MIX_IDX));
    parameters_.back()->SetMacroRole(MacroRole::MIX);

    Update();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void CompressorEffect::Update() {
    threshold_db_  = parameters_[kParamThreshold]->GetValue();
    ratio_         = std::max(1.0f, parameters_[kParamRatio]->GetValue());
    const float attack_ms  = parameters_[kParamAttack]->GetValue();
    const float release_ms = parameters_[kParamRelease]->GetValue();
    attack_coeff_  = AttackCoeff(attack_ms, sampleRate_);
    release_coeff_ = ReleaseCoeff(release_ms, sampleRate_);
    const float makeup_db  = parameters_[kParamMakeup]->GetValue();
    makeup_lin_    = std::powf(10.0f, makeup_db / 20.0f);
    mix_           = Clamp01(parameters_[kParamMix]->GetValue());
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void CompressorEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        const float abs_in = std::fabsf(in[i]);

        // Envelope follower with separate attack/release
        if (abs_in > envelope_) {
            envelope_ = attack_coeff_ * envelope_ + (1.0f - attack_coeff_) * abs_in;
        } else {
            envelope_ = release_coeff_ * envelope_ + (1.0f - release_coeff_) * abs_in;
        }

        const float gain = ComputeGain(envelope_) * makeup_lin_;
        const float wet  = in[i] * gain;
        out[i] = wet * mix_ + in[i] * (1.0f - mix_);
    }
}

// ---------------------------------------------------------------------------
// ProcessStereo — stereo-linked: gain reduction from the louder channel
// ---------------------------------------------------------------------------

void CompressorEffect::ProcessStereo(const float* inL, const float* inR,
                                     float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        const float absL = std::fabsf(inL[i]);
        const float absR = std::fabsf(inR[i]);

        // Track each channel independently
        if (absL > envelopeL_) {
            envelopeL_ = attack_coeff_ * envelopeL_ + (1.0f - attack_coeff_) * absL;
        } else {
            envelopeL_ = release_coeff_ * envelopeL_ + (1.0f - release_coeff_) * absL;
        }

        if (absR > envelopeR_) {
            envelopeR_ = attack_coeff_ * envelopeR_ + (1.0f - attack_coeff_) * absR;
        } else {
            envelopeR_ = release_coeff_ * envelopeR_ + (1.0f - release_coeff_) * absR;
        }

        // Stereo-linked: use the louder of the two for gain computation
        const float linkedLevel = std::max(envelopeL_, envelopeR_);
        const float gain = ComputeGain(linkedLevel) * makeup_lin_;

        outL[i] = (inL[i] * gain) * mix_ + inL[i] * (1.0f - mix_);
        outR[i] = (inR[i] * gain) * mix_ + inR[i] * (1.0f - mix_);
    }
}
