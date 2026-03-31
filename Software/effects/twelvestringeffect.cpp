#include "twelvestringeffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"

#include <cmath>
#include <algorithm>

using namespace perspective;

// PitchShifter contains DelayLine<float,16384>[2] = 128 KB. Place in external SDRAM.
static daisysp::PitchShifter DSY_SDRAM_BSS g_pitchShifterPool;

TwelveStringEffect::TwelveStringEffect()
    : Effect("12 String")
    , pitchShifter_(&g_pitchShifterPool)
    , writeIndex_(0)
    , lfoPhaseL_(0.0f)
    , lfoPhaseR_(0.25f)
    , octaveMix_(0.35f)
    , detuneDepth_(0.6f)
    , detuneRate_(0.4f)
    , chorusMix_(0.6f)
    , outputLevel_(1.0f)
{
}

TwelveStringEffect::~TwelveStringEffect() {
}

void TwelveStringEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    pitchShifter_->Init(sampleRate);
    pitchShifter_->SetDelSize(4800); // ~100ms at 48kHz, down from default 341ms
    pitchShifter_->SetTransposition(12.0f);
    pitchShifter_->SetFun(0.0f);

    for (int i = 0; i < kChorusBufferSize; ++i) {
        chorusBufferL_[i] = 0.0f;
        chorusBufferR_[i] = 0.0f;
    }

    // K1: Octave mix — how much of the +12 layer to blend in
    AddParameter(new PotentiometerParameter("K1 Octave", 0.0f, 1.0f, 0.35f, PotCurve::LIN, KNOB_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    // K2: Detune depth — chorus sweep width (gives the paired-string shimmer)
    AddParameter(new PotentiometerParameter("K2 Detune", 0.0f, 1.0f, 0.6f, PotCurve::LIN, KNOB_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    // K3: Detune rate — chorus LFO rate in Hz
    AddParameter(new PotentiometerParameter("K3 Rate", 0.05f, 2.0f, 0.4f, PotCurve::LOG, KNOB_3_IDX));

    // K4: Chorus mix — wet level of the detuned layer
    AddParameter(new PotentiometerParameter("K4 Chorus", 0.0f, 1.0f, 0.6f, PotCurve::LIN, KNOB_4_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    // K5: Output level trim
    AddParameter(new PotentiometerParameter("K5 Level", 0.0f, 1.0f, 0.85f, PotCurve::LIN, KNOB_5_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(100.0f);

    Update();
}

float TwelveStringEffect::ReadDelayInterp(const float* buf, float readPos) const {
    while (readPos < 0.0f)                          readPos += kChorusBufferSize;
    while (readPos >= (float)kChorusBufferSize)     readPos -= kChorusBufferSize;

    int   x1   = static_cast<int>(readPos);
    float frac = readPos - static_cast<float>(x1);
    int   x0   = (x1 - 1 + kChorusBufferSize) % kChorusBufferSize;
    int   x2   = (x1 + 1) % kChorusBufferSize;
    int   x3   = (x1 + 2) % kChorusBufferSize;
    return CubicInterp(buf[x0], buf[x1], buf[x2], buf[x3], frac);
}

void TwelveStringEffect::Update() {
    if (parameters_.size() < 5) return;

    octaveMix_   = parameters_[kParamOctaveMix]->GetValue();
    detuneDepth_ = parameters_[kParamDetuneDepth]->GetValue();
    detuneRate_  = parameters_[kParamDetuneRate]->GetValue();
    chorusMix_   = parameters_[kParamChorusMix]->GetValue();
    outputLevel_ = parameters_[kParamOutputLevel]->GetValue();
}

void TwelveStringEffect::OnSelected() {
    // The PitchShifter object lives in SDRAM BSS, which the startup code does NOT
    // zero-initialise (only internal SRAM BSS is zeroed in Reset_Handler). Init()
    // correctly clears the delay-line buffers via Reset(), but leaves several
    // metadata members untouched: slewed_mod_[], mod_coeff_[], prev_phs_a_/b_,
    // mod_a_amt_/b_amt_.  If SDRAM contains garbage at those locations and
    // mod_coeff_ happens to be near float-max, the first audio frame computes
    //   slewed_mod_ += garbage_coeff * (0 - garbage_slew)  →  overflow → Inf
    // then  delay.Read() = 0 + (0-0) * Inf = 0 * Inf = NaN,
    // which permanently poisons both the pitch-shifter output and the chorus
    // buffers, producing the audible noise burst on first selection.
    // Zeroing the entire object before Init() eliminates all SDRAM garbage.
    std::fill(reinterpret_cast<char*>(pitchShifter_),
              reinterpret_cast<char*>(pitchShifter_) + sizeof(*pitchShifter_),
              '\0');

    pitchShifter_->Init(sampleRate_);
    pitchShifter_->SetDelSize(4800);
    pitchShifter_->SetTransposition(12.0f);
    pitchShifter_->SetFun(0.0f);

    for (int i = 0; i < kChorusBufferSize; ++i) {
        chorusBufferL_[i] = 0.0f;
        chorusBufferR_[i] = 0.0f;
    }
    writeIndex_ = 0;
    lfoPhaseL_  = 0.0f;
    lfoPhaseR_  = 0.25f;
}

// Mono Process — falls through to stereo with duplicated channel
void TwelveStringEffect::Process(float* in, float* out, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
        return;
    }
    ProcessStereo(in, in, out, out, size);
}

void TwelveStringEffect::ProcessStereo(float* inL, float* inR,
                                       float* outL, float* outR, size_t size) {
    if (!enabled_) {
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    const float phaseInc     = detuneRate_ / sampleRate_;
    const float baseDelayS   = kBaseDelayMs  * 0.001f * sampleRate_;
    const float maxSweepS    = kMaxSweepMs   * 0.001f * sampleRate_;

    for (size_t i = 0; i < size; ++i) {
        float dryL = inL[i];
        float dryR = inR[i];

        // --- Octave layer (+12 semitones) — mono shift, stereo spread via chorus ---
        float monoIn = (dryL + dryR) * 0.5f;
        float shifted = pitchShifter_->Process(monoIn);

        // Mix: dry + octave layer
        float mixedL = dryL + shifted * octaveMix_;
        float mixedR = dryR + shifted * octaveMix_;

        // --- Chorus detuning (simulates the slight pitch/time offset between paired strings) ---
        chorusBufferL_[writeIndex_] = mixedL;
        chorusBufferR_[writeIndex_] = mixedR;

        float lfoL = std::sinf(2.0f * kPi * lfoPhaseL_);
        float lfoR = std::sinf(2.0f * kPi * lfoPhaseR_);

        // Sweep around base delay
        float delaySamplesL = baseDelayS + detuneDepth_ * maxSweepS * lfoL;
        float delaySamplesR = baseDelayS + detuneDepth_ * maxSweepS * lfoR;

        float readPosL = static_cast<float>(writeIndex_) - delaySamplesL;
        float readPosR = static_cast<float>(writeIndex_) - delaySamplesR;

        float wetL = ReadDelayInterp(chorusBufferL_, readPosL);
        float wetR = ReadDelayInterp(chorusBufferR_, readPosR);

        // Blend: (1 - chorusMix) * straight mix + chorusMix * detuned version
        float finalL = mixedL * (1.0f - chorusMix_) + wetL * chorusMix_;
        float finalR = mixedR * (1.0f - chorusMix_) + wetR * chorusMix_;

        outL[i] = finalL * outputLevel_;
        outR[i] = finalR * outputLevel_;

        writeIndex_ = (writeIndex_ + 1) % kChorusBufferSize;
        lfoPhaseL_  = WrapPhase(lfoPhaseL_ + phaseInc);
        lfoPhaseR_  = WrapPhase(lfoPhaseR_ + phaseInc);
    }
}
