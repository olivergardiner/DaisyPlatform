#ifndef PERSPECTIVE_DELAYEFFECT_H
#define PERSPECTIVE_DELAYEFFECT_H

#include "tempoeffect.h"
#include "daisysp.h"

using namespace daisysp;

namespace perspective {

constexpr size_t MAX_DELAY = 48000 * 4; // 4 seconds max delay at 48kHz
constexpr size_t MAX_DELAY_INSTANCES = 8; // Maximum number of DelayEffect instances

class DelayEffect : public TempoEffect {
public:
    DelayEffect();
    ~DelayEffect() override;

    void Init(float sampleRate) override;
    void Process(float* in, float* out, size_t size) override;
    void ProcessStereo(float* inL, float* inR, float* outL, float* outR, size_t size) override;
    void Update() override;
    void OnSelected() override;
    void OnDeselected() override;

private:
    float currentDelaySamples_ = 0.0f;  // Cache current delay time in samples
    
    // Pointer to delay lines (allocated from static pool) - kept valid to avoid race conditions
    DelayLine<float, MAX_DELAY>* delayL_;
    DelayLine<float, MAX_DELAY>* delayR_;
    int instanceId_; // Which instance from the pool this is using (-1 if not allocated)
    volatile bool delayBuffersAllocated_; // Flag to indicate if buffers are currently allocated

    bool TryAllocateDelayLines();
    void ReleaseDelayLines();
};

} // namespace perspective

#endif // PERSPECTIVE_DELAYEFFECT_H
