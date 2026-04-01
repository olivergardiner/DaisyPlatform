#include "pitchshiftereffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"
#include "../parameters/encoderparameter.h"

#include <cmath>
#include <cstring>

using namespace perspective;

// Per-channel circular delay buffers in external SDRAM.
// kBufSize floats × 2 channels × 4 bytes = 32 KB total.
static float DSY_SDRAM_BSS g_bufL[PitchShifterEffect::kBufSize];
static float DSY_SDRAM_BSS g_bufR[PitchShifterEffect::kBufSize];

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PitchShifterEffect::PitchShifterEffect()
    : Effect("Pitch Shift")
    , bufL_(g_bufL)
    , bufR_(g_bufR)
    , pitchRatio_(1.0f)
    , semitones_(0.0f)
    , mix_(0.5f)
{
    stateL_ = {};
    stateR_ = {};
}

PitchShifterEffect::~PitchShifterEffect() {}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void PitchShifterEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;
    pitchRatio_ = 1.0f;

    ClearBuffers();
    InitChannelState(stateL_);
    InitChannelState(stateR_);

    AddParameter(new EncoderParameter("E1 Semitones", -12.0f, 12.0f, 0.0f, 1.0f, ENCODER_1_IDX));
    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));

    Update();
}

// ---------------------------------------------------------------------------
// OnSelected — SDRAM is not zero-initialised at boot; reset everything clean
// ---------------------------------------------------------------------------

void PitchShifterEffect::OnSelected() {
    ClearBuffers();
    InitChannelState(stateL_);
    InitChannelState(stateR_);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void PitchShifterEffect::Update() {
    semitones_  = parameters_[kParamSemitones]->GetValue();
    mix_        = parameters_[kParamMix]->GetValue();
    pitchRatio_ = powf(2.0f, semitones_ / 12.0f);
}

// ---------------------------------------------------------------------------
// InitChannelState
//
// Stagger the 4 grain phases evenly (0, 0.25, 0.5, 0.75) so there is always
// at least one grain near its peak amplitude. Position each grain's read
// pointer as if it has already run for phi[g] of its first cycle, so the
// engine produces output immediately rather than waiting grainSize samples
// for the first grain reset.
// ---------------------------------------------------------------------------

void PitchShifterEffect::InitChannelState(ChannelState& s) {
    s.writeIdx = 0;

    for (int g = 0; g < kNumGrains; ++g) {
        s.phi[g] = static_cast<float>(g) / static_cast<float>(kNumGrains);

        // Start position: kStartDelay behind writeIdx=0, then advance by
        // phi[g]*kGrainSize*pitchRatio_ to match the grain's mid-flight state.
        // Buffers are zeroed so reading from any position is safe (gives 0).
        float pos = static_cast<float>(kBufSize - kStartDelay)
                    + s.phi[g] * static_cast<float>(kGrainSize) * pitchRatio_;

        while (pos >= static_cast<float>(kBufSize)) pos -= static_cast<float>(kBufSize);
        if (pos < 0.0f) pos += static_cast<float>(kBufSize);

        s.readPos[g] = pos;
    }
}

// ---------------------------------------------------------------------------
// ProcessSample — core per-sample OLA engine (one channel)
//
// The write pointer advances by 1 per output sample.
// Each grain's read pointer advances by pitchRatio_ per output sample.
//   pitchRatio_ > 1  →  read faster than write  →  pitch up
//   pitchRatio_ < 1  →  read slower than write  →  pitch down
//
// When a grain phase wraps from 1→0 (every kGrainSize output samples), the
// read pointer is hard-reset to kStartDelay samples behind the current write
// head. The Hann window is 0 at both grain boundaries (φ=0 and φ=1), so
// the jump in read position is inaudible.
//
// Hann sum: Σ sin²(π(φ + k/4)) for k=0..3 = 2.0 for all φ.
// Output × 0.5 normalises to unity passthrough.
// ---------------------------------------------------------------------------

float PitchShifterEffect::ProcessSample(float input, float* buf, ChannelState& s) {
    buf[s.writeIdx & kBufMask] = input;
    s.writeIdx++;

    float out = 0.0f;
    static constexpr float kPhaseInc = 1.0f / static_cast<float>(kGrainSize);

    for (int g = 0; g < kNumGrains; ++g) {
        out += Hann(s.phi[g]) * ReadInterp(buf, s.readPos[g]);

        // Advance read pointer at the pitch ratio (independent of write speed)
        s.readPos[g] += pitchRatio_;
        if (s.readPos[g] >= static_cast<float>(kBufSize)) {
            s.readPos[g] -= static_cast<float>(kBufSize);
        }

        // Advance grain phase; on wrap, hard-reset read pointer
        s.phi[g] += kPhaseInc;
        if (s.phi[g] >= 1.0f) {
            s.phi[g] -= 1.0f;
            // Reset: read from kStartDelay samples behind current write head
            s.readPos[g] = static_cast<float>(
                (s.writeIdx - kStartDelay + kBufSize) & kBufMask);
        }
    }

    return out * 0.5f;  // Hann sum = 2; scale to unity
}

// ---------------------------------------------------------------------------
// Process (mono)
// ---------------------------------------------------------------------------

void PitchShifterEffect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }

    const float wet = mix_;
    const float dry = 1.0f - wet;

    for (size_t i = 0; i < size; ++i) {
        float shifted = ProcessSample(in[i], bufL_, stateL_);
        out[i] = dry * in[i] + wet * shifted;
    }
}

// ---------------------------------------------------------------------------
// ProcessStereo — L and R have independent grain state and buffers
// ---------------------------------------------------------------------------

void PitchShifterEffect::ProcessStereo(const float* inL, const float* inR,
                                        float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }

    const float wet = mix_;
    const float dry = 1.0f - wet;

    for (size_t i = 0; i < size; ++i) {
        float shL = ProcessSample(inL[i], bufL_, stateL_);
        float shR = ProcessSample(inR[i], bufR_, stateR_);
        outL[i] = dry * inL[i] + wet * shL;
        outR[i] = dry * inR[i] + wet * shR;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void PitchShifterEffect::ClearBuffers() {
    memset(bufL_, 0, sizeof(float) * kBufSize);
    memset(bufR_, 0, sizeof(float) * kBufSize);
}

