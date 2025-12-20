// Example usage of the UI Event Handler System

#include "uieventhandler.h"
#include "../hardware.h"

using namespace perspective;

// Example 1: Simple knob change callback
void OnKnobChanged(const UIEvent& event) {
    // Access knob information
    int knobIndex = event.controlIndex;  // 0-based index
    
    // React to the change
    if (event.name == "Knob_1") {
        // Do something when first knob changes
        float newValue = event.value;
        float oldValue = event.previousValue;
        // ... handle knob change ...
        // You can now map this to an effect parameter if needed
    }
}

// Example 2: Button press callback
void OnButtonPressed(const UIEvent& event) {
    int buttonIndex = event.controlIndex;  // 0-based index
    
    if (event.name == "Switch_1") {
        // Toggle effect bypass
        // ... handle bypass button ...
    }
}

// Example 3: Encoder callback
void OnEncoderTurned(const UIEvent& event) {
    int encoderIndex = event.controlIndex;  // 0-based index
    int increment = event.intValue;  // Positive=CW, Negative=CCW
    
    if (increment > 0) {
        // Encoder turned clockwise
        // ... handle encoder increment ...
    } else if (increment < 0) {
        // Encoder turned counter-clockwise
        // ... handle encoder decrement ...
    }
}

// Example 4: Using lambda callbacks
void SetupEventHandlers() {
    UIEventHandler eventHandler;
    
    // Register a lambda for all knob changes
    eventHandler.RegisterListener(
        [](const UIEvent& event) {
            // This will be called for ANY knob change
            int knobIndex = event.controlIndex;
            float value = event.value;
            // ... map to effect parameters as needed ...
        },
        UIEventType::KNOB_CHANGED
    );
        
    // Register for all button presses
    eventHandler.RegisterListener(OnButtonPressed, UIEventType::BUTTON_PRESSED);
    
    // Register for encoder events
    eventHandler.RegisterListener(OnEncoderTurned, UIEventType::ENCODER_CHANGED);
}

// Example 5: Using the queue system (interrupt-safe)
void InterruptSafeExample() {
    UIEventHandler eventHandler;
    Hardware hardware;
    
    // Register listeners
    eventHandler.RegisterListener(OnButtonPressed, UIEventType::BUTTON_PRESSED);
    eventHandler.RegisterListener(OnKnobChanged, UIEventType::KNOB_CHANGED);
    
    // Hardware automatically calls event handler methods from timer interrupt
    // This could be called from an interrupt or hardware callback
    // Events are queued, not processed immediately
    
    // Example of manually queuing a knob event
    // float previousValue = 0.5f;
    // float newValue = 0.6f;
    // eventHandler.QueueKnobChanged(&hardware.knobs[0], 0, newValue, previousValue, "Knob_1");
    
    // Button interrupt handler example
    // void OnButtonInterrupt() {
    //     eventHandler.QueueButtonPressed(&myButton, 0, "Switch_1");
    // }
    
    // In your main loop, process all queued events
    // This is where the callbacks actually get called
    while (true) {
        // Process hardware, update parameters, etc.
        // ...
        
        // Process all queued UI events
        eventHandler.ProcessEvents();
        
        // Check queue size if needed
        if (eventHandler.GetQueueSize() > 0) {
            // Still have events to process
        }
        
        // Continue main loop...
    }
}
