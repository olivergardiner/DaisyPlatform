
#ifndef PERSPECTIVE_PERSPECTIVE_H
#define PERSPECTIVE_PERSPECTIVE_H

#include "hardware.h"
#include "parameters/effectparameter.h"
#include "parameters/potentiometerparameter.h"
#include "parameters/encoderparameter.h"
#include "parameters/toggleparameter.h"
#include "ui/ui.h"
#include "effects/tunereffect.h"
#include "preset.h"

#include <vector>

namespace perspective {

class Effect;  // Forward declaration

enum class BypassType {
    TRUE_BYPASS,    // Hardware bypass via relay (drive pin low)
    PASSTHROUGH     // Pass audio through unprocessed
};

enum class PerspectiveMode {
    EFFECT,
    TUNER,
    SETTINGS,
    PRESET
};


class Perspective : public UI {
public:
    Perspective();
    ~Perspective() override;

    int GetMetronomeMode() const { return metronomeMode_; }

    void Init() override;
    void Exec() override;
    void AudioCallbackImpl(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);
    void SetCurrentEffect(size_t index);

protected:
    void RegisterEventListeners();
    void LoadEffects();
    void ToggleBypass();
    void ToggleBypassType();
    void HandleTapTempo();
    void ToggleMetronome();
    void EnterTunerMode();
    void ExitTunerMode();
    void UpdateTunerDisplay();
    void EnterSettingsMode();
    void ExitSettingsMode();
    void EnterPresetMode();
    void ExitPresetMode();
    void UpdatePresetDisplay();
    void LoadPresetAtIndex(size_t slot);
    void CacheCurrentEffect();
    void RestoreCachedEffect();
    void EnterPresetEditMode();
    void ExitPresetEditMode();
    void ExecutePresetEditAction();
    void UpdateParameterDisplay(EffectParameter* param, size_t displayIndex);
    void UpdateParameterDisplayHighlighted(EffectParameter* param, size_t displayIndex);
    void UpdateStatusDisplay();

    Hardware hardware;
    Effect* currentEffect_ = nullptr;
    std::vector<Effect*> effects_;
    size_t currentEffectIndex_ = 0;

    // Settings parameters (unified model)
    std::vector<EffectParameter*> settingsParameters_;
    // Indices for settings parameters
    static constexpr int kSettingsParamTuningReference = 0;
    static constexpr int kSettingsParamMetronomeLevel = 1;
    static constexpr int kSettingsParamMetronomeMode = 2;

    // Metronome mode (0 = Bass, 1 = Snare, 2 = Click, 3 = High)
    int metronomeMode_ = 0;

    bool bypassMode_ = false;
    BypassType bypassType_ = BypassType::PASSTHROUGH;
    bool switchingEffect_ = false;
    PerspectiveMode mode_ = PerspectiveMode::EFFECT;
    TunerEffect* tunerEffect_ = nullptr;
    bool tunerSw1ReleasedOnce_ = false;  // true once sw1 has been released after hold-to-enter
    uint32_t lastTunerDisplayTime_ = 0;  // timestamp of last tuner display update
    bool metronomeEnabled_ = false;  // Global metronome state
    float metronomeLevel_ = 0.7f;    // Global metronome volume (0.0 - 1.0)

    // Tap tempo state
    uint32_t lastTapTime_ = 0;
    uint32_t tapInterval_ = 0;
    static constexpr uint32_t TAP_TIMEOUT_MS = 2000;  // Reset if no tap within 2 seconds

    // Preset state
    enum class PresetEditOption { SAVE, OVERWRITE, DELETE, CANCEL, COUNT };
    PresetBank presetBank_;
    size_t currentPresetSlot_ = 0;
    size_t cachedEffectIndex_ = 0;
    float cachedParamValues_[PRESET_MAX_PARAMS] = {};
    size_t cachedParamCount_ = 0;
    bool presetMuted_ = true;       // Mute when empty preset selected
    bool presetEditMode_ = false;   // Encoder 2 edit sub-mode
    int presetEditSelection_ = 0;   // Current edit option index
};

} // namespace perspective

#endif // PERSPECTIVE_PERSPECTIVE_H