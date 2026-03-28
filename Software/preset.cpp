#include "preset.h"
#include <cstring>

using namespace perspective;

PresetBank::PresetBank() {
    Clear();
}

void PresetBank::Clear() {
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        ClearSlot(i);
    }
}

void PresetBank::ClearSlot(size_t slot) {
    if (slot >= PRESET_COUNT) return;
    presets_[slot].occupied = false;
    presets_[slot].effectIndex = 0;
    presets_[slot].name[0] = '\0';
    presets_[slot].paramCount = 0;
    for (size_t i = 0; i < PRESET_MAX_PARAMS; i++) {
        presets_[slot].params[i].value = 0.0f;
    }
}

bool PresetBank::IsOccupied(size_t slot) const {
    if (slot >= PRESET_COUNT) return false;
    return presets_[slot].occupied;
}

const PresetData& PresetBank::Get(size_t slot) const {
    return presets_[slot < PRESET_COUNT ? slot : 0];
}

void PresetBank::Save(size_t slot, size_t effectIndex, const char* name, const float* paramValues, size_t paramCount) {
    if (slot >= PRESET_COUNT) return;
    PresetData& p = presets_[slot];
    p.occupied = true;
    p.effectIndex = effectIndex;
    strncpy(p.name, name, PRESET_NAME_LEN - 1);
    p.name[PRESET_NAME_LEN - 1] = '\0';
    p.paramCount = paramCount < PRESET_MAX_PARAMS ? paramCount : PRESET_MAX_PARAMS;
    for (size_t i = 0; i < p.paramCount; i++) {
        p.params[i].value = paramValues[i];
    }
}
