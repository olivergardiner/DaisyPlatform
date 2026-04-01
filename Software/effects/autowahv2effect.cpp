#include "autowahv2effect.h"

#include "../controls.h"

#include <cmath>

using namespace perspective;
using namespace daisysp;

namespace {

static constexpr float kMinDetectorGain = 5.0f;
static constexpr float kMaxDetectorGain = 9.0f;
static constexpr float kAttackMinSec = 0.001f;
static constexpr float kAttackMaxSec = 0.5f;

static constexpr float kDetectorHpCutoffHz = 120.0f;
static constexpr float kCutoffSlewSec = 0.005f;

static constexpr float kMinCutoffHz = 20.0f;
static constexpr float kMaxCutoffHz = 5000.0f;
static constexpr float kDownSweepFloorHz = 100.0f;

enum VoiceMode {
    kVoiceClassic = 0,
    kVoiceClassicLinear = 1,
    kVoiceRefined = 2,
    kVoiceRefinedLinear = 3,
};

static const char* kVoiceLabels[4] = {
    "C Log", "C Lin", "R Log", "R Lin"
};

static const char* kDownBoostLabels[4] = {
    "Off", "Subtle", "Strong", "Max"
};

static constexpr float kDetectorRawMixRefined = 0.65f;
static constexpr float kDetectorHpMixRefined = 0.35f;
static constexpr float kEnvelopeDriveBoostClassic = 1.25f;
static constexpr float kEnvelopeDriveBoostRefined = 1.15f;
static constexpr float kDownSweepBiasClassic[4] = {1.0f, 1.10f, 1.20f, 1.30f};
static constexpr float kDownSweepBiasRefined[4] = {1.0f, 1.06f, 1.12f, 1.18f};

static float ClampDetectorValue(float x, float lo, float hi) {
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

static float DetectorGainFromAttack(float attackTimeSec) {
    float clampedAttack = ClampDetectorValue(attackTimeSec, kAttackMinSec, kAttackMaxSec);
    float normalized = (clampedAttack - kAttackMinSec) / (kAttackMaxSec - kAttackMinSec);
    return kMinDetectorGain + normalized * (kMaxDetectorGain - kMinDetectorGain);
}

} // namespace

AutowahV2Effect::AutowahV2Effect()
    : AutowahV2Effect("Autowah V2") {
}

AutowahV2Effect::AutowahV2Effect(const char* name)
    : Effect(name)
    , envelope_(0.0f)
    , detectorInput_(0.0f)
    , detectorLpStateL_(0.0f)
    , detectorLpStateR_(0.0f)
    , cutoffSmoothedHz_(1000.0f)
    , detectorHpAlpha_(0.01f)
    , cutoffSlewAlpha_(0.01f) {
}

AutowahV2Effect::~AutowahV2Effect() {
}

float AutowahV2Effect::ClampValue(float x, float lo, float hi) {
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

float AutowahV2Effect::OnePoleAlpha(float cutoffHz, float sampleRate) {
    float safeCutoff = ClampValue(cutoffHz, 1.0f, 0.45f * sampleRate);
    return 1.0f - expf(-2.0f * 3.14159265359f * safeCutoff / sampleRate);
}

float AutowahV2Effect::TimeConstantAlpha(float timeSec, float sampleRate) {
    float safeTime = ClampValue(timeSec, 0.0001f, 2.0f);
    return 1.0f - expf(-1.0f / (sampleRate * safeTime));
}

void AutowahV2Effect::InitFilterState(float sampleRate, float initialCutoffHz) {
    sampleRate_ = sampleRate;

    filterL_.Init(sampleRate_);
    filterR_.Init(sampleRate_);

    envelope_ = 0.0f;
    detectorInput_ = 0.0f;
    detectorLpStateL_ = 0.0f;
    detectorLpStateR_ = 0.0f;
    cutoffSmoothedHz_ = ClampValue(initialCutoffHz, kMinCutoffHz, kMaxCutoffHz);

    detectorHpAlpha_ = OnePoleAlpha(kDetectorHpCutoffHz, sampleRate_);
    cutoffSlewAlpha_ = TimeConstantAlpha(kCutoffSlewSec, sampleRate_);
}

void AutowahV2Effect::ProcessWahSample(float input, float mix, float resonance, float targetCutoffHz, float& output, bool useCompensatedResonance) {
    cutoffSmoothedHz_ += cutoffSlewAlpha_ * (targetCutoffHz - cutoffSmoothedHz_);

    float effectiveRes = useCompensatedResonance ? ComputeCompensatedResonance(resonance, cutoffSmoothedHz_) : ClampValue(resonance, 0.0f, 1.0f);
    filterL_.SetRes(effectiveRes);
    filterL_.SetFreq(cutoffSmoothedHz_);

    filterL_.Process(input);
    float wet = filterL_.Band();
    output = input * (1.0f - mix) + wet * mix;
}

void AutowahV2Effect::ProcessWahStereoSample(float inputL, float inputR, float mix, float resonance, float targetCutoffHz, float& outputL, float& outputR, bool useCompensatedResonance) {
    cutoffSmoothedHz_ += cutoffSlewAlpha_ * (targetCutoffHz - cutoffSmoothedHz_);

    float effectiveRes = useCompensatedResonance ? ComputeCompensatedResonance(resonance, cutoffSmoothedHz_) : ClampValue(resonance, 0.0f, 1.0f);
    filterL_.SetRes(effectiveRes);
    filterR_.SetRes(effectiveRes);
    filterL_.SetFreq(cutoffSmoothedHz_);
    filterR_.SetFreq(cutoffSmoothedHz_);

    filterL_.Process(inputL);
    filterR_.Process(inputR);

    float wetL = filterL_.Band();
    float wetR = filterR_.Band();

    outputL = inputL * (1.0f - mix) + wetL * mix;
    outputR = inputR * (1.0f - mix) + wetR * mix;
}

void AutowahV2Effect::Init(float sampleRate) {
    InitFilterState(sampleRate, 1000.0f);

    AddParameter(new PotentiometerParameter("K1 Mix", 0.0f, 1.0f, 0.5f, PotCurve::LIN, KNOB_1_IDX));
    AddParameter(new PotentiometerParameter("K2 Resonance", 0.0f, 1.0f, 0.85f, PotCurve::LIN, KNOB_2_IDX));
    AddParameter(new PotentiometerParameter("K3 Frequency", 400.0f, 2000.0f, 1000.0f, PotCurve::LOG, KNOB_3_IDX));

    AddParameter(new PotentiometerParameter("K4 Attack ms", 0.001f, 1.0f, 0.2f, PotCurve::LOG, KNOB_4_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(1000.0f);

    AddParameter(new PotentiometerParameter("K5 Release ms", 0.001f, 0.5f, 0.01f, PotCurve::LOG, KNOB_5_IDX));
    parameters_.back()->SetDisplayType(DisplayType::SCALED);
    parameters_.back()->SetScaleFactor(1000.0f);

    AddParameter(new PotentiometerParameter("K6 Sensitivity", -3000.0f, 3000.0f, 1600.0f, PotCurve::LIN, KNOB_6_IDX));

    AddParameter(new EncoderParameter("E1 Voice", 0.0f, 3.0f, static_cast<float>(kVoiceRefined), 1.0f, ENCODER_1_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kVoiceLabels, 4);

    AddParameter(new EncoderParameter("E2 Down+", 0.0f, 3.0f, 1.0f, 1.0f, ENCODER_2_IDX));
    parameters_.back()->SetDisplayType(DisplayType::DISCRETE);
    parameters_.back()->SetDiscreteValues(kDownBoostLabels, 4);

    Update();
}

float AutowahV2Effect::ProcessDetectorHighpass(float input, float& lowpassState) const {
    lowpassState += detectorHpAlpha_ * (input - lowpassState);
    return input - lowpassState;
}

float AutowahV2Effect::ComputeTargetCutoffHz(float baseFreq, float sensitivityHz, float envelope, bool useLogCurve) const {
    float endFreq = baseFreq + sensitivityHz;
    float minFreq = sensitivityHz < 0.0f ? kDownSweepFloorHz : kMinCutoffHz;

    float f0 = ClampValue(baseFreq, minFreq, kMaxCutoffHz);
    float f1 = ClampValue(endFreq, minFreq, kMaxCutoffHz);

    if (fabsf(f1 - f0) < 0.001f) {
        return f0;
    }

    float clampedEnv = ClampValue(envelope, 0.0f, 1.0f);

    if (!useLogCurve) {
        float targetLinear = f0 + ((f1 - f0) * clampedEnv);
        return ClampValue(targetLinear, minFreq, kMaxCutoffHz);
    }

    float ratio = f1 / f0;
    if (ratio <= 0.0f) {
        return f0;
    }

    // Log-frequency sweep sounds more natural than linear-Hz mapping.
    float target = f0 * powf(ratio, clampedEnv);
    return ClampValue(target, minFreq, kMaxCutoffHz);
}

float AutowahV2Effect::ComputeCompensatedResonance(float resonance, float cutoffHz) const {
    float normalized = ClampValue(cutoffHz / kMaxCutoffHz, 0.0f, 1.0f);

    // Keep perceived Q more consistent across the sweep range.
    float compensation = 0.75f + 0.5f * sqrtf(normalized);
    return ClampValue(resonance * compensation, 0.0f, 1.0f);
}

void AutowahV2Effect::Process(const float* in, float* out, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            out[i] = in[i];
        }
        return;
    }

    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float resonance = parameters_.size() >= 6 ? parameters_[kParamResonance]->GetValue() : 0.85f;
    float baseFreq = parameters_.size() >= 6 ? parameters_[kParamFrequency]->GetValue() : 1000.0f;
    float attackSec = parameters_.size() >= 6 ? parameters_[kParamAttackMs]->GetValue() : 0.2f;
    float releaseSec = parameters_.size() >= 6 ? parameters_[kParamReleaseMs]->GetValue() : 0.01f;
    float sensitivityHz = parameters_.size() >= 6 ? parameters_[kParamSensitivity]->GetValue() : 1600.0f;
    int voiceMode = parameters_.size() >= 7 ? static_cast<int>(std::round(parameters_[kParamVoice]->GetValue())) : kVoiceRefined;
    voiceMode = static_cast<int>(ClampValue(static_cast<float>(voiceMode), 0.0f, 3.0f));
    int downBoostMode = parameters_.size() >= 8 ? static_cast<int>(std::round(parameters_[kParamDownBoost]->GetValue())) : 1;
    downBoostMode = static_cast<int>(ClampValue(static_cast<float>(downBoostMode), 0.0f, 3.0f));

    float detectorGain = DetectorGainFromAttack(attackSec);
    float attackAlpha = TimeConstantAlpha(attackSec, sampleRate_);
    float releaseAlpha = TimeConstantAlpha(releaseSec, sampleRate_);

    bool refinedVoice = (voiceMode == kVoiceRefined || voiceMode == kVoiceRefinedLinear);
    bool logCurve = (voiceMode == kVoiceClassic || voiceMode == kVoiceRefined);
    float driveBoost = refinedVoice ? kEnvelopeDriveBoostRefined : kEnvelopeDriveBoostClassic;
    float downSweepBias = refinedVoice ? kDownSweepBiasRefined[downBoostMode] : kDownSweepBiasClassic[downBoostMode];

    for (size_t i = 0; i < size; ++i) {
        float detectorHp = ProcessDetectorHighpass(in[i], detectorLpStateL_);
        float detectorRaw = fabsf(in[i]);
        float detectorSource = refinedVoice
            ? ((kDetectorRawMixRefined * detectorRaw) + (kDetectorHpMixRefined * fabsf(detectorHp)))
            : detectorRaw;

        // Blend raw + high-passed detector to keep pick response while rejecting low-end pumping.
        float detectorInput = ClampValue(detectorSource * detectorGain, 0.0f, 1.0f);
        detectorInput_ = detectorInput;

        if (detectorInput > envelope_) {
            envelope_ += attackAlpha * (detectorInput - envelope_);
        } else {
            envelope_ += releaseAlpha * (detectorInput - envelope_);
        }

        float drivenEnvelope = ClampValue(envelope_ * detectorGain * driveBoost, 0.0f, 1.0f);
        if (sensitivityHz < 0.0f) {
            drivenEnvelope = ClampValue(drivenEnvelope * downSweepBias, 0.0f, 1.0f);
        }
        float shapedEnvelope = refinedVoice ? sqrtf(drivenEnvelope) : drivenEnvelope;
        float targetCutoff = ComputeTargetCutoffHz(baseFreq, sensitivityHz, shapedEnvelope, logCurve);

        ProcessWahSample(in[i], mix, resonance, targetCutoff, out[i], refinedVoice);
    }
}

void AutowahV2Effect::ProcessStereo(const float* inL, const float* inR, float* outL, float* outR, size_t size) {
    if (!enabled_) {
        detectorInput_ = 0.0f;
        for (size_t i = 0; i < size; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    float mix = parameters_.size() > 0 ? parameters_[kParamMix]->GetValue() : 0.5f;
    float resonance = parameters_.size() >= 6 ? parameters_[kParamResonance]->GetValue() : 0.85f;
    float baseFreq = parameters_.size() >= 6 ? parameters_[kParamFrequency]->GetValue() : 1000.0f;
    float attackSec = parameters_.size() >= 6 ? parameters_[kParamAttackMs]->GetValue() : 0.2f;
    float releaseSec = parameters_.size() >= 6 ? parameters_[kParamReleaseMs]->GetValue() : 0.01f;
    float sensitivityHz = parameters_.size() >= 6 ? parameters_[kParamSensitivity]->GetValue() : 1600.0f;
    int voiceMode = parameters_.size() >= 7 ? static_cast<int>(std::round(parameters_[kParamVoice]->GetValue())) : kVoiceRefined;
    voiceMode = static_cast<int>(ClampValue(static_cast<float>(voiceMode), 0.0f, 3.0f));
    int downBoostMode = parameters_.size() >= 8 ? static_cast<int>(std::round(parameters_[kParamDownBoost]->GetValue())) : 1;
    downBoostMode = static_cast<int>(ClampValue(static_cast<float>(downBoostMode), 0.0f, 3.0f));

    float detectorGain = DetectorGainFromAttack(attackSec);
    float attackAlpha = TimeConstantAlpha(attackSec, sampleRate_);
    float releaseAlpha = TimeConstantAlpha(releaseSec, sampleRate_);

    bool refinedVoice = (voiceMode == kVoiceRefined || voiceMode == kVoiceRefinedLinear);
    bool logCurve = (voiceMode == kVoiceClassic || voiceMode == kVoiceRefined);
    float driveBoost = refinedVoice ? kEnvelopeDriveBoostRefined : kEnvelopeDriveBoostClassic;
    float downSweepBias = refinedVoice ? kDownSweepBiasRefined[downBoostMode] : kDownSweepBiasClassic[downBoostMode];

    for (size_t i = 0; i < size; ++i) {
        float hpL = ProcessDetectorHighpass(inL[i], detectorLpStateL_);
        float hpR = ProcessDetectorHighpass(inR[i], detectorLpStateR_);
        float rawL = fabsf(inL[i]);
        float rawR = fabsf(inR[i]);
        float detectorRaw = 0.5f * (rawL + rawR);
        float detectorHp = 0.5f * (fabsf(hpL) + fabsf(hpR));
        float detectorSource = refinedVoice
            ? ((kDetectorRawMixRefined * detectorRaw) + (kDetectorHpMixRefined * detectorHp))
            : detectorRaw;

        float detectorInput = ClampValue(detectorSource * detectorGain, 0.0f, 1.0f);
        detectorInput_ = detectorInput;

        if (detectorInput > envelope_) {
            envelope_ += attackAlpha * (detectorInput - envelope_);
        } else {
            envelope_ += releaseAlpha * (detectorInput - envelope_);
        }

        float drivenEnvelope = ClampValue(envelope_ * detectorGain * driveBoost, 0.0f, 1.0f);
        if (sensitivityHz < 0.0f) {
            drivenEnvelope = ClampValue(drivenEnvelope * downSweepBias, 0.0f, 1.0f);
        }
        float shapedEnvelope = refinedVoice ? sqrtf(drivenEnvelope) : drivenEnvelope;
        float targetCutoff = ComputeTargetCutoffHz(baseFreq, sensitivityHz, shapedEnvelope, logCurve);
        ProcessWahStereoSample(inL[i], inR[i], mix, resonance, targetCutoff, outL[i], outR[i], refinedVoice);
    }
}

void AutowahV2Effect::Update() {
    if (parameters_.size() >= 6) {
        // Resonance is compensated per-sample in Process as cutoff moves.
        // Keep current value valid by clamping the current smoothed cutoff.
        cutoffSmoothedHz_ = ClampValue(cutoffSmoothedHz_, kMinCutoffHz, kMaxCutoffHz);
    }
}

float AutowahV2Effect::GetEnvelopeBrightness() const {
    return detectorInput_;
}
