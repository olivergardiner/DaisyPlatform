#ifndef PERSPECTIVE_TOGGLEPARAMETER_H
#define PERSPECTIVE_TOGGLEPARAMETER_H

#include "effectparameter.h"

namespace perspective {

// Subclass for toggle parameters (on/off switches)
class ToggleParameter : public EffectParameter {
public:
    ToggleParameter(const char* name, bool defaultValue = false, int index = -1, const char* trueStr = "On", const char* falseStr = "Off", int displayIndex = -2);
    virtual ~ToggleParameter();

    // Toggle the state
    void Toggle();
    
    // Get boolean state
    bool GetState() const;
    
    // Set boolean state
    void SetState(bool state);
    void SetValue(float value) override;
    
    ParameterType GetType() const override;
    void GetValueAsString(char* buffer, size_t bufferSize) const override;

private:
    bool state_;
    char trueStr_[8];
    char falseStr_[8];
};

} // namespace perspective

#endif // PERSPECTIVE_TOGGLEPARAMETER_H
