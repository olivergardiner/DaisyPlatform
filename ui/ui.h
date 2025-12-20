#pragma once

#include "uieventhandler.h"

namespace perspective {
    
class UI {
public:
    UI();
    virtual ~UI();
    
    virtual void Init() = 0;  // Pure virtual - must be implemented by subclass
    
    void Exec();
    
    UIEventHandler* GetEventHandler();
    
protected:
    UIEventHandler eventHandler_;
};

} // namespace perspective