#ifndef PERSPECTIVE_UIEVENTHANDLER_H
#define PERSPECTIVE_UIEVENTHANDLER_H

#include <functional>
#include <vector>
#include <string>
#include <queue>

namespace perspective {

// Event types
enum class UIEventType {
    KNOB_CHANGED,
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_HELD,
    ENCODER_CHANGED
};

// Event data structure
struct UIEvent {
    UIEventType type;
    void* source;           // Pointer to the control that triggered the event
    int controlIndex;       // Index of the control (0-based)
    int value;              // Current value (for knobs)
    int previousValue;      // Previous value (for change detection)
    
    UIEvent()
        : type(UIEventType::KNOB_CHANGED)
        , source(nullptr)
        , controlIndex(-1)
        , value(0)
        , previousValue(0)
    {}
};

// Callback function type
using UIEventCallback = std::function<void(const UIEvent&)>;

// Event listener structure
struct UIEventListener {
    UIEventCallback callback;
    UIEventType eventType;
    void* source;  // nullptr means listen to all sources
    int controlIndex;  // -1 means listen to all control indices
    
    UIEventListener(UIEventCallback cb, UIEventType type, void* src = nullptr, int ctrlIdx = -1)
        : callback(cb)
        , eventType(type)
        , source(src)
        , controlIndex(ctrlIdx)
    {}
};

// Event handler system
class UIEventHandler {
public:
    UIEventHandler();
    ~UIEventHandler();
    
    // Register a callback for a specific event type
    void RegisterListener(UIEventCallback callback, UIEventType eventType, void* source = nullptr);
    
    // Register a callback for a specific event type and control index
    void RegisterListenerByIndex(UIEventCallback callback, UIEventType eventType, int controlIndex);
    
    // Queue an event (can be called from interrupts)
    void QueueEvent(const UIEvent& event);
    
    // Process all queued events (call from main loop)
    void ProcessEvents();
    
    // Get number of events in queue
    size_t GetQueueSize() const;
    
    // Clear the event queue
    void ClearQueue();
    
    // Create and queue events (interrupt-safe)
    void QueueKnobChanged(void* knob, int controlIndex, int value, int previousValue);
    void QueueButtonPressed(void* button, int controlIndex);
    void QueueButtonReleased(void* button, int controlIndex);
    void QueueButtonHeld(void* button, int controlIndex, float holdTimeMs);
    void QueueEncoderChanged(void* encoder, int controlIndex, int increment);
    
    // Remove all listeners
    void ClearListeners();
    
    // Remove listeners for a specific source
    void RemoveListenersForSource(void* source);
    
    // Queue delegates
    void PushEvent(const UIEvent& event);
    UIEvent PopEvent();

private:
    std::vector<UIEventListener> listeners_;
    std::queue<UIEvent> eventQueue_;
    
    // Internal method to dispatch a single event to listeners
    void DispatchEventToListeners(const UIEvent& event);
};

} // namespace perspective

#endif // PERSPECTIVE_UIEVENTHANDLER_H
