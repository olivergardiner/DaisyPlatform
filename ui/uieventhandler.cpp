#include "uieventhandler.h"
#include "knob.h"

#include <algorithm>

using namespace perspective;

UIEventHandler::UIEventHandler() {}

UIEventHandler::~UIEventHandler() {
    ClearListeners();
}

void UIEventHandler::RegisterListener(UIEventCallback callback, UIEventType eventType, void* source) {
    listeners_.emplace_back(callback, eventType, source, -1);
}

void UIEventHandler::RegisterListenerByIndex(UIEventCallback callback, UIEventType eventType, int controlIndex) {
    listeners_.emplace_back(callback, eventType, nullptr, controlIndex);
}

void UIEventHandler::PushEvent(const UIEvent& event) {
    eventQueue_.push(event);
}

UIEvent UIEventHandler::PopEvent() {
    UIEvent event = eventQueue_.front();
    eventQueue_.pop();
    return event;
}

void UIEventHandler::QueueEvent(const UIEvent& event) {
    PushEvent(event);
}

void UIEventHandler::ProcessEvents() {
    while (!eventQueue_.empty()) {
        UIEvent event = PopEvent();
        if (event.type == UIEventType::KNOB_CHANGED) {
            Knob *knob = static_cast<Knob*>(event.source);
            knob->Filter();
        }
        DispatchEventToListeners(event);
    }
}

size_t UIEventHandler::GetQueueSize() const {
    return eventQueue_.size();
}

void UIEventHandler::ClearQueue() {
    while (!eventQueue_.empty()) {
        PopEvent();
    }
}

void UIEventHandler::DispatchEventToListeners(const UIEvent& event) {
    for (const auto& listener : listeners_) {
        // Check if listener matches event type
        if (listener.eventType == event.type) {
            // Check if listener is for this specific source or all sources
            bool sourceMatches = (listener.source == nullptr || listener.source == event.source);
            // Check if listener is for this specific control index or all indices
            bool indexMatches = (listener.controlIndex == -1 || listener.controlIndex == event.controlIndex);
            
            if (sourceMatches && indexMatches) {
                listener.callback(event);
            }
        }
    }
}

void UIEventHandler::QueueKnobChanged(void* knob, int controlIndex, int value, int previousValue) {
    UIEvent event;
    event.type = UIEventType::KNOB_CHANGED;
    event.source = knob;
    event.controlIndex = controlIndex;
    event.value = value;
    event.previousValue = previousValue;
    QueueEvent(event);
}

void UIEventHandler::QueueButtonPressed(void* button, int controlIndex) {
    UIEvent event;
    event.type = UIEventType::BUTTON_PRESSED;
    event.source = button;
    event.controlIndex = controlIndex;
    QueueEvent(event);
}

void UIEventHandler::QueueButtonReleased(void* button, int controlIndex) {
    UIEvent event;
    event.type = UIEventType::BUTTON_RELEASED;
    event.source = button;
    event.controlIndex = controlIndex;
    QueueEvent(event);
}

void UIEventHandler::QueueButtonHeld(void* button, int controlIndex, float holdTimeMs) {
    UIEvent event;
    event.type = UIEventType::BUTTON_HELD;
    event.source = button;
    event.controlIndex = controlIndex;
    event.value = holdTimeMs;
    QueueEvent(event);
}

void UIEventHandler::QueueEncoderChanged(void* encoder, int controlIndex, int increment) {
    UIEvent event;
    event.type = UIEventType::ENCODER_CHANGED;
    event.source = encoder;
    event.controlIndex = controlIndex;
    event.value = increment;  // Positive for CW, negative for CCW
    QueueEvent(event);
}

void UIEventHandler::ClearListeners() {
    listeners_.clear();
}

void UIEventHandler::RemoveListenersForSource(void* source) {
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
            [source](const UIEventListener& listener) {
                return listener.source == source;
            }),
        listeners_.end()
    );
}
