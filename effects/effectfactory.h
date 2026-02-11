#ifndef PERSPECTIVE_EFFECTFACTORY_H
#define PERSPECTIVE_EFFECTFACTORY_H

#include <vector>
#include "../effect.h"

// Include all effect types
#include "autowaheffect.h"
#include "bandpasseffect.h"
#include "choruseffect.h"
#include "delayeffect.h"
#include "flangereffect.h"
#include "phasereffect.h"
#include "tunereffect.h"
#include "waheffect.h"

namespace perspective {

/**
 * @brief Populates a vector with all available effects
 * @param effects Pointer to vector to populate with effect instances
 * @param sampleRate Sample rate to initialize effects with
 */
inline void PopulateEffects(std::vector<Effect*>* effects, float sampleRate) {
    if (!effects) return;
    
    // Add delay effect
    DelayEffect* delayEffect = new DelayEffect();
    delayEffect->Init(sampleRate);
    effects->push_back(delayEffect);
    
    // Add chorus effect
    /*ChorusEffect* chorusEffect = new ChorusEffect();
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
    
    // Add wah effect
    WahEffect* wahEffect = new WahEffect();
    wahEffect->Init(sampleRate);
    effects->push_back(wahEffect);
    
    // Add autowah effect
    AutowahEffect* autowahEffect = new AutowahEffect();
    autowahEffect->Init(sampleRate);
    effects->push_back(autowahEffect);
    
    // Add bandpass effect
    BandpassEffect* bandpassEffect = new BandpassEffect();
    bandpassEffect->Init(sampleRate);
    effects->push_back(bandpassEffect);
    
    // Add tuner effect
    TunerEffect* tunerEffect = new TunerEffect();
    tunerEffect->Init(sampleRate);
    effects->push_back(tunerEffect);*/
}

} // namespace perspective

#endif // PERSPECTIVE_EFFECTFACTORY_H
