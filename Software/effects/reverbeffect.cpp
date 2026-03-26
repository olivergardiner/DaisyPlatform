#include "reverbeffect.h"

#include "../controls.h"

#include <algorithm>

using namespace perspective;
using namespace daisysp;

namespace {

static constexpr size_t kMaxReverbInstances = 1;

static daisysp::ReverbSc DSY_SDRAM_BSS reverbPool_[kMaxReverbInstances];
static bool reverbPoolUsed_[kMaxReverbInstances] = {false};

static int AllocateReverbInstance() {
    for (size_t i = 0; i < kMaxReverbInstances; ++i) {
        if (!reverbPoolUsed_[i]) {
            reverbPoolUsed_[i] = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void FreeReverbInstance(int id) {
    if (id >= 0 && static_cast<size_t>(id) < kMaxReverbInstances) {
        reverbPoolUsed_[id] = false;
    }
}

static float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

} // namespace

ReverbEffect::ReverbEffect()
    : Effect("Reverb")
    , reverb_(nullptr)
    , mix_(0.15f)
    , feedback_(0.82f)
    , lpFreq_(2800.0f)
    , instanceId_(-1)
    , reverbAllocated_(false) {
}

ReverbEffect::~ReverbEffect() {
    ReleaseReverb();
}

bool ReverbEffect::TryAllocateReverb() {
    if (reverbAllocated_) {
        return true;
    }

    instanceId_ = AllocateReverbInstance();
    if (instanceId_ < 0) {
        return false;
    }

    reverb_ = &reverbPool_[instanceId_];
    reverb_->Init(sampleRate_);
    reverbAllocated_ = true;
    return true;
}

void ReverbEffect::ReleaseReverb() {
    if (instanceId_ >= 0) {
        FreeReverbInstance(instanceId_);
        instanceId_ = -1;
    }
    reverbAllocated_ = false;
}

void ReverbEffect::Init(float sampleRate) {
    sampleRate_ = sampleRate;

    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.15f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Feedback", 0.0f, 1.0f, 0.82f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Cutoff Hz", 200.0f, 12000.0f, 2800.0f, PotCurve::LOG, KNOB_3_IDX));

    Update();
}

void ReverbEffect::OnSelected() {
    TryAllocateReverb();
    Update();
}

void ReverbEffect::OnDeselected() {
    ReleaseReverb();
}

void ReverbEffect::Process(float* in, float* out, size_t size) {
    ProcessStereo(in, in, out, out, size);
}

void ReverbEffect::ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_ || !reverbAllocated_ || !reverb_) {
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        float wetL = 0.0f;
        float wetR = 0.0f;
        reverb_->Process(inL[i], inR[i], &wetL, &wetR);

        outL[i] = inL[i] * (1.0f - mix_) + wetL * mix_;
        outR[i] = inR[i] * (1.0f - mix_) + wetR * mix_;
    }
}

void ReverbEffect::Update() {
    if (parameters_.size() < 3) {
        return;
    }

    float mix = Clamp01(parameters_[kParamMix]->GetValue());
    float feedback = Clamp01(parameters_[kParamFeedback]->GetValue());
    float lpFreq = parameters_[kParamCutoffHz]->GetValue();

    SetMix(mix);
    SetFeedback(feedback);
    SetLpFreq(lpFreq);

    if (!reverbAllocated_ || !reverb_) {
        return;
    }

    reverb_->SetFeedback(Clamp01(feedback_));
    reverb_->SetLpFreq(std::max(200.0f, std::min(lpFreq_, sampleRate_ * 0.45f)));
}

void ReverbEffect::SetMix(float mix) {
    mix_ = Clamp01(mix);
}

void ReverbEffect::SetFeedback(float feedback) {
    feedback_ = Clamp01(feedback);
}

void ReverbEffect::SetLpFreq(float lpFreq) {
    lpFreq_ = lpFreq;
}