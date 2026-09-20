#include "cabsimeffect.h"

#include "../controls.h"
#include "../parameters/valueparameter.h"

#include <algorithm>
#include <cmath>

using namespace perspective;

namespace {

static constexpr float kLowCutQ = 0.80f;
static constexpr float kResonanceQ = 1.40f;
static constexpr float kPresenceQ = 1.10f;

} // namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

CabSimEffect::CabSimEffect()
    : Effect("Cab Sim")
    , lowCut_hz_(85.0f)
    , resonance_db_(3.0f)
    , presence_db_(4.0f)
    , rolloff_hz_(4200.0f)
    , level_lin_(1.0f) {
}

CabSimEffect::~CabSimEffect() {
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void CabSimEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    AddParameter(new ValueParameter("Low Cut", 40.0f, 200.0f, 85.0f));
    AddParameter(new ValueParameter("Reso", -6.0f, 9.0f, 3.0f));
    AddParameter(new ValueParameter("Presence", -6.0f, 9.0f, 4.0f));
    AddParameter(new ValueParameter("Rolloff", 2500.0f, 7000.0f, 4200.0f));
    AddParameter(new ValueParameter("Level", -12.0f, 12.0f, 0.0f));

    // Force an initial design pass
    lowCut_hz_ = resonance_db_ = presence_db_ = rolloff_hz_ = 1e9f;
    Update();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void CabSimEffect::Update() {
    const float lowCut = parameters_[kParamLowCut]->GetValue();
    const float reso = parameters_[kParamResonance]->GetValue();
    const float presence = parameters_[kParamPresence]->GetValue();
    const float rolloff = parameters_[kParamRolloff]->GetValue();

    if (std::fabsf(lowCut - lowCut_hz_) > 0.5f) {
        lowCut_hz_ = lowCut;
        for (Channel& c : channels_) c.lowCut.SetHighpass(lowCut_hz_, kLowCutQ, sampleRate_);
    }

    if (std::fabsf(reso - resonance_db_) > 0.01f) {
        resonance_db_ = reso;
        for (Channel& c : channels_) c.resonance.SetPeaking(kResonanceFreq, kResonanceQ, resonance_db_, sampleRate_);
    }

    if (std::fabsf(presence - presence_db_) > 0.01f) {
        presence_db_ = presence;
        for (Channel& c : channels_) c.presence.SetPeaking(kPresenceFreq, kPresenceQ, presence_db_, sampleRate_);
    }

    if (std::fabsf(rolloff - rolloff_hz_) > 0.5f) {
        rolloff_hz_ = rolloff;
        for (Channel& c : channels_) {
            c.rolloffA.SetLowpass(rolloff_hz_, kButterQ1, sampleRate_);
            c.rolloffB.SetLowpass(rolloff_hz_, kButterQ2, sampleRate_);
        }
    }

    level_lin_ = std::powf(10.0f, parameters_[kParamLevel]->GetValue() / 20.0f);
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void CabSimEffect::ProcessChannel(Channel& channel, const float* in, float* out, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        float v = channel.lowCut.Process(in[i]);
        v = channel.resonance.Process(v);
        v = channel.presence.Process(v);
        v = channel.rolloffA.Process(v);
        v = channel.rolloffB.Process(v);
        out[i] = v * level_lin_;
    }
}

void CabSimEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    ProcessChannel(channels_[0], in, out, size);
}

void CabSimEffect::ProcessStereo(const float* inL, const float* inR,
                                 float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    ProcessChannel(channels_[0], inL, outL, size);
    ProcessChannel(channels_[1], inR, outR, size);
}
