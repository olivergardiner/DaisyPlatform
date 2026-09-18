#ifndef PERSPECTIVE_EFFECTFACTORY_H
#define PERSPECTIVE_EFFECTFACTORY_H

#include <vector>
#include "../platform.h"
#include "effect.h"

// Include all effect types
#include "autowaheffect.h"
#include "autowahv2effect.h"
#include "cabsimeffect.h"
#include "choruseffect.h"
#include "compoundeffect.h"
#include "compressoreffect.h"
#include "delayeffect.h"
#include "driveeffect.h"
#include "noisegateeffect.h"
#include "sandmaneffect.h"
#include "tonestackeffect.h"
#include "flangereffect.h"
#include "flyeffect.h"
#include "motorwaheffect.h"
#include "mysteriouseffect.h"
#include "moddelayeffect.h"
#include "paralleldelayeffect.h"
#include "phasereffect.h"
#include "tremoloeffect.h"
#include "reverbeffect.h"
#include "slapbackdelayeffect.h"
#include "streetseffect.h"
#include "tunereffect.h"
#include "twelvestringeffect.h"
#include "waheffect.h"

namespace perspective {

/**
 * @brief Populates a vector with all available effects
 * @param effects Pointer to vector to populate with effect instances
 * @param sampleRate Sample rate to initialize effects with
 */
inline void PopulateEffects(std::vector<Effect*>* effects, float sampleRate) {
    if (!effects) return;

    Hardware::PrintLine("Populating effects...");
    
    // Add delay effect
    DelayEffect* delayEffect = new DelayEffect();
    delayEffect->Init(sampleRate);
    effects->push_back(delayEffect);
    
    // Add mod delay effect
    ModDelayEffect* modDelayEffect = new ModDelayEffect();
    modDelayEffect->Init(sampleRate);
    effects->push_back(modDelayEffect);
    
    // Add slapback delay effect
    SlapbackDelayEffect* slapbackDelayEffect = new SlapbackDelayEffect();
    slapbackDelayEffect->Init(sampleRate);
    effects->push_back(slapbackDelayEffect);

    // Add reverb effect
    ReverbEffect* reverbEffect = new ReverbEffect();
    reverbEffect->Init(sampleRate);
    effects->push_back(reverbEffect);
    
    // Add parallel delay effect
    ParallelDelayEffect* parallelDelayEffect = new ParallelDelayEffect();
    parallelDelayEffect->Init(sampleRate);
    effects->push_back(parallelDelayEffect);
    
    // Add chorus effect
    ChorusEffect* chorusEffect = new ChorusEffect();
    chorusEffect->Init(sampleRate);
    effects->push_back(chorusEffect);
    
    // Add flanger effect
    FlangerEffect* flangerEffect = new FlangerEffect();
    flangerEffect->Init(sampleRate);
    effects->push_back(flangerEffect);
    
    // Add phaser effect
    PhaserEffect* phaserEffect = new PhaserEffect();
    phaserEffect->Init(sampleRate);
    effects->push_back(phaserEffect);

    // Add tremolo effect
    TremoloEffect* tremoloEffect = new TremoloEffect();
    tremoloEffect->Init(sampleRate);
    effects->push_back(tremoloEffect);
    
    // Add autowah effect
    AutowahEffect* autowahEffect = new AutowahEffect();
    autowahEffect->Init(sampleRate);
    effects->push_back(autowahEffect);

    // Add improved autowah effect
    AutowahV2Effect* autowahV2Effect = new AutowahV2Effect();
    autowahV2Effect->Init(sampleRate);
    effects->push_back(autowahV2Effect);

    // Add LFO-driven autowah effect
    MotorWahEffect* motorWahEffect = new MotorWahEffect();
    motorWahEffect->Init(sampleRate);
    effects->push_back(motorWahEffect);
    
    // Add wah effect
    WahEffect* wahEffect = new WahEffect();
    wahEffect->Init(sampleRate);
    effects->push_back(wahEffect);

    // Add 12-string guitar emulation effect
    TwelveStringEffect* twelveStringEffect = new TwelveStringEffect();
    twelveStringEffect->Init(sampleRate);
    effects->push_back(twelveStringEffect);

    // Add compressor effect
    CompressorEffect* compressorEffect = new CompressorEffect();
    compressorEffect->Init(sampleRate);
    effects->push_back(compressorEffect);

    // --- Compound effects below this line ---

    // Add streets effect
    StreetsEffect* streetsEffect = new StreetsEffect();
    streetsEffect->Init(sampleRate);
    effects->push_back(streetsEffect);

    // Add Fly compound effect (AutoWahV2 + Delay 3/16ths)
    FlyEffect* flyEffect = new FlyEffect();
    flyEffect->Init(sampleRate);
    effects->push_back(flyEffect);

    // Add mysterious compound effect
    MysteriousEffect* mysteriousEffect = new MysteriousEffect();
    mysteriousEffect->Init(sampleRate);
    effects->push_back(mysteriousEffect);

#if defined(PERSPECTIVE_PLATFORM_AMP)
    // --- Amp chain (mono FX + cab sim platform only) ---
    // These effects keep single-channel filter and envelope state, so they must
    // not be driven through the default Effect::ProcessStereo(). See platform.h.
    //
    // NB: presets are persisted to flash by effect index, so new effects must
    // be appended here. Inserting one above this point would silently remap
    // every saved preset to a different effect.

    // Add noise gate effect
    NoiseGateEffect* noiseGateEffect = new NoiseGateEffect();
    noiseGateEffect->Init(sampleRate);
    effects->push_back(noiseGateEffect);

    // Add drive effect (cascaded asymmetric stages, 2x oversampled)
    DriveEffect* driveEffect = new DriveEffect();
    driveEffect->Init(sampleRate);
    effects->push_back(driveEffect);

    // Add tone stack effect
    ToneStackEffect* toneStackEffect = new ToneStackEffect();
    toneStackEffect->Init(sampleRate);
    effects->push_back(toneStackEffect);

    // NB: no CabSimEffect here. On the amp platform the cab sim is a fixture
    // on channel 2, owned by Perspective and always in circuit, so registering
    // it as a selectable effect as well would only let you cab the signal twice.

    // Add Sandman compound effect (gate + drive + tone stack)
    SandmanEffect* sandmanEffect = new SandmanEffect();
    sandmanEffect->Init(sampleRate);
    effects->push_back(sandmanEffect);
#endif // PERSPECTIVE_PLATFORM_AMP

    Hardware::PrintLine("Done populating effects.");
}

} // namespace perspective

#endif // PERSPECTIVE_EFFECTFACTORY_H
