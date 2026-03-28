#ifndef PERSPECTIVE_PRESET_H
#define PERSPECTIVE_PRESET_H

#include <cstddef>
#include <cstdint>

namespace perspective {

static constexpr size_t PRESET_COUNT = 16;
static constexpr size_t PRESET_MAX_PARAMS = 16;
static constexpr size_t PRESET_NAME_LEN = 24;

struct PresetParamData {
    float value;
};

struct PresetData {
    bool occupied;
    size_t effectIndex;
    char name[PRESET_NAME_LEN];
    size_t paramCount;
    PresetParamData params[PRESET_MAX_PARAMS];
};

class PresetBank {
public:
    PresetBank();

    void Clear();
    void ClearSlot(size_t slot);
    bool IsOccupied(size_t slot) const;
    const PresetData& Get(size_t slot) const;
    void Save(size_t slot, size_t effectIndex, const char* name, const float* paramValues, size_t paramCount);

private:
    PresetData presets_[PRESET_COUNT];
};

} // namespace perspective

#endif // PERSPECTIVE_PRESET_H
