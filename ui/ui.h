#ifndef PERSPECTIVE_UI_H
#define PERSPECTIVE_UI_H

#include "uieventhandler.h"

namespace perspective {
    
class UI {
public:
    UI();
    virtual ~UI();
    
    virtual void Init() = 0;  // Pure virtual - must be implemented by subclass
    
    virtual void Exec();
    
    UIEventHandler* GetEventHandler();
    
protected:
    UIEventHandler eventHandler_;
};

} // namespace perspective

#endif // PERSPECTIVE_UI_H