#include "tonestackeffect.h"

#include "../controls.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kBassFreq = 120.0f;
static constexpr float kTrebleFreq = 3000.0f;
static constexpr float kShelfSlope = 0.70f;
static constexpr float kMidQ = 0.80f;

} // namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

ToneStackEffect::ToneStackEffect()
    : Effect("Tone Stack")
    , bass_db_(0.0f)
    , mid_db_(0.0f)
    , mid_freq_(800.0f)
    , treble_db_(0.0f)
    , level_lin_(1.0f) {
}

ToneStackEffect::~ToneStackEffect() {
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void ToneStackEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    AddParameter(new PotentiometerParameter("Bass", -15.0f, 15.0f, 0.0f, PotCurve::LIN, -1));
    AddParameter(new PotentiometerParameter("Mid", -15.0f, 15.0f, 0.0f, PotCurve::LIN, -1));
    AddParameter(new PotentiometerParameter("Mid Hz", 300.0f, 2000.0f, 800.0f, PotCurve::LOG, -1));
    AddParameter(new PotentiometerParameter("Treble", -15.0f, 15.0f, 0.0f, PotCurve::LIN, -1));
    // Wide trim range: as the last stage of a high-gain chain this carries the
    // whole preset's output level, so it needs to cut further than a tone
    // control normally would.
    AddParameter(new PotentiometerParameter("Level", -24.0f, 12.0f, 0.0f, PotCurve::LIN, -1));

    // Force an initial design pass
    bass_db_ = mid_db_ = treble_db_ = 1e9f;
    Update();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void ToneStackEffect::Update() {
    const float bass_db = parameters_[kParamBass]->GetValue();
    const float mid_db = parameters_[kParamMid]->GetValue();
    const float mid_freq = parameters_[kParamMidFreq]->GetValue();
    const float treble_db = parameters_[kParamTreble]->GetValue();

    if (std::fabsf(bass_db - bass_db_) > 0.01f) {
        bass_db_ = bass_db;
        for (Channel& c : channels_) c.bass.SetLowShelf(kBassFreq, kShelfSlope, bass_db_, sampleRate_);
    }

    if (std::fabsf(mid_db - mid_db_) > 0.01f || std::fabsf(mid_freq - mid_freq_) > 0.5f) {
        mid_db_ = mid_db;
        mid_freq_ = mid_freq;
        for (Channel& c : channels_) c.mid.SetPeaking(mid_freq_, kMidQ, mid_db_, sampleRate_);
    }

    if (std::fabsf(treble_db - treble_db_) > 0.01f) {
        treble_db_ = treble_db;
        for (Channel& c : channels_) c.treble.SetHighShelf(kTrebleFreq, kShelfSlope, treble_db_, sampleRate_);
    }

    level_lin_ = std::powf(10.0f, parameters_[kParamLevel]->GetValue() / 20.0f);
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void ToneStackEffect::ProcessChannel(Channel& channel, const float* in, float* out, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        float v = channel.bass.Process(in[i]);
        v = channel.mid.Process(v);
        v = channel.treble.Process(v);
        out[i] = v * level_lin_;
    }
}

void ToneStackEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    ProcessChannel(channels_[0], in, out, size);
}

void ToneStackEffect::ProcessStereo(const float* inL, const float* inR,
                                    float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    ProcessChannel(channels_[0], inL, outL, size);
    ProcessChannel(channels_[1], inR, outR, size);
}
