#include "ui.h"

using namespace perspective;

UI::UI() {

}

UI::~UI() {
    
}

void UI::Exec() {
    while(true) {
        eventHandler_.ProcessEvents();
    }
}

UIEventHandler* UI::GetEventHandler() {
    return &eventHandler_;
}
