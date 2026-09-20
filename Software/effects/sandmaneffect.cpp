#include "sandmaneffect.h"

#include "noisegateeffect.h"
#include "driveeffect.h"
#include "tonestackeffect.h"
#include "../controls.h"
#include "../parameters/potentiometerparameter.h"

using namespace perspective;

namespace {

static const char* kStageNames[] = {"2", "3", "4"};

// Child parameter indices, mirroring each child's own ParamIndex enum.
enum GateParam { kGateThreshold = 0, kGateHysteresis, kGateAttack, kGateHold, kGateRelease };
enum DriveParam { kDriveGain = 0, kDriveStages, kDriveAsym, kDriveScoop, kDriveScoopFreq, kDriveLevel };
enum ToneParam { kToneBass = 0, kToneMid, kToneMidFreq, kToneTreble, kToneLevel };

} // namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

SandmanEffect::SandmanEffect()
    : CompoundEffect("Sandman", RoutingMode::SERIES)
    , gate_(nullptr)
    , drive_(nullptr)
    , tone_(nullptr) {
}

SandmanEffect::~SandmanEffect() {
    // Children are cleaned up by the CompoundEffect destructor
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void SandmanEffect::Init(float sampleRate) {
    drive_ = new DriveEffect();
    AddEffect(drive_);

    tone_ = new ToneStackEffect();
    AddEffect(tone_);

    // Gate last. Ahead of the cascade it could barely control the output at
    // all: with this much gain the drive simply re-amplifies whatever the gate
    // attenuates, so the gate has to slam fully shut before anything audible
    // happens, and it does nothing about the noise the cascade makes itself.
    //
    // After the tone stack rather than before it, so nothing downstream can
    // ring on once the gate has closed.
    //
    // This only works because the detector is keyed from the pedal input (see
    // Effect::SetKeyInput) — a level detector looking at the cascade's own
    // output would be watching a signal compressed to within a few dB of full
    // scale, with no usable dynamic range to trigger on.
    gate_ = new NoiseGateEffect();
    AddEffect(gate_);

    // Initializes the children and allocates the series temp buffers
    CompoundEffect::Init(sampleRate);

    // K1: Level — output trim
    AddParameter(new PotentiometerParameter("K1 Level", -18.0f, 6.0f, -3.0f, PotCurve::LIN, MACRO_KNOB_MIX_IDX));
    parameters_.back()->SetMacroRole(MacroRole::MIX);

    // K2: Gain — total pre-gain into the cascade
    AddParameter(new PotentiometerParameter("K2 Gain", 12.0f, 48.0f, 34.0f, PotCurve::LIN, MACRO_KNOB_DEPTH_IDX));
    parameters_.back()->SetMacroRole(MacroRole::DEPTH);

    // Gate threshold — the muted chugs live or die on this one
    AddParameter(new PotentiometerParameter("Gate", -80.0f, -20.0f, -50.0f, PotCurve::LIN, -1));

    // Scoop — mid dip ahead of each clipping stage
    AddParameter(new PotentiometerParameter("Scoop", 0.0f, 18.0f, 10.0f, PotCurve::LIN, -1));

    // Treble — tone stack high shelf. Cab presence is a global setting now,
    // so top-end shaping inside the preset happens here.
    AddParameter(new PotentiometerParameter("Treble", -15.0f, 15.0f, 3.5f, PotCurve::LIN, -1));

    // Stages — 3 for the album grind, 2 if it feels too compressed
    PotentiometerParameter* stagesParam =
        new PotentiometerParameter("Stages", 0.0f, 2.0f, 1.0f, PotCurve::LIN, -1);
    stagesParam->SetDisplayType(DisplayType::DISCRETE);
    stagesParam->SetDiscreteValues(kStageNames, 3);
    AddParameter(stagesParam);

    Update();
}

// ---------------------------------------------------------------------------
// Update — push the macros down into the children, hold the rest fixed
// ---------------------------------------------------------------------------

void SandmanEffect::Update() {
    if (GetParameterCount() < 6) {
        CompoundEffect::Update();
        return;
    }

    const float level_db = GetParameter(kParamLevel)->GetValue();
    const float gain_db = GetParameter(kParamGain)->GetValue();
    const float gate_db = GetParameter(kParamGate)->GetValue();
    const float scoop_db = GetParameter(kParamScoop)->GetValue();
    const float treble_db = GetParameter(kParamTreble)->GetValue();
    const float stages = GetParameter(kParamStages)->GetValue();

    // Gate: fast open, short hold, quick release. Long enough not to chatter on
    // a sustained note, short enough that a palm mute stops dead.
    //
    // These are unchanged by the gate sitting last rather than first: the
    // detector keys off the pedal input either way, so the same thresholds and
    // timings apply. What changed is only what the gain ramp multiplies.
    if (gate_ && gate_->GetParameterCount() >= 5) {
        gate_->GetParameter(kGateThreshold)->SetValue(gate_db);
        gate_->GetParameter(kGateHysteresis)->SetValue(7.0f);
        gate_->GetParameter(kGateAttack)->SetValue(0.8f);
        gate_->GetParameter(kGateHold)->SetValue(30.0f);
        gate_->GetParameter(kGateRelease)->SetValue(60.0f);
    }

    // Drive: the scoop sits low, around 600 Hz, which is what hollows out the
    // mids without thinning the low end the riff depends on.
    if (drive_ && drive_->GetParameterCount() >= 6) {
        drive_->GetParameter(kDriveGain)->SetValue(gain_db);
        drive_->GetParameter(kDriveStages)->SetValue(stages);
        drive_->GetParameter(kDriveAsym)->SetValue(0.5f);
        drive_->GetParameter(kDriveScoop)->SetValue(scoop_db);
        drive_->GetParameter(kDriveScoopFreq)->SetValue(600.0f);
        drive_->GetParameter(kDriveLevel)->SetValue(-8.0f);
    }

    // Tone stack: a little low-end lift, a narrower dip higher up than the
    // drive's scoop, and the Treble macro on top. Also carries the output trim,
    // since this is now the last stage in the chain.
    if (tone_ && tone_->GetParameterCount() >= 5) {
        tone_->GetParameter(kToneBass)->SetValue(3.0f);
        tone_->GetParameter(kToneMid)->SetValue(-5.0f);
        tone_->GetParameter(kToneMidFreq)->SetValue(900.0f);
        tone_->GetParameter(kToneTreble)->SetValue(treble_db);
        tone_->GetParameter(kToneLevel)->SetValue(level_db);
    }

    CompoundEffect::Update();
}

// ---------------------------------------------------------------------------
// LED feedback — follow the drive stage
// ---------------------------------------------------------------------------

float SandmanEffect::GetEnvelopeBrightness() const {
    return drive_ ? drive_->GetEnvelopeBrightness() : 0.0f;
}
