# Perspective Audio Pedal

Audio effects pedal firmware for the Daisy Seed platform.

## Hardware Platform

- **Microcontroller**: Daisy Seed (STM32H750 ARM Cortex-M7)
- **Framework**: libDaisy + DaisySP

## Features

- **Hardware Abstraction Layer**: Clean interface for potentiometers, switches, encoders, and LEDs
- **UI Event System**: Interrupt-safe queue-based event handling for responsive user interaction
- **Effect Framework**: Base effect class with parameter management
- **Parameter System**: Support for multiple potentiometer curves (linear, logarithmic, A/W tapers, exponential, etc.)

### Core Components

- **Hardware** (`hardware.h/cpp`): Main hardware abstraction managing all controls and peripherals
- **UI** (`ui.h/cpp`): User interface manager with event handler integration
- **UIEventHandler** (`uieventhandler.h/cpp`): Queue-based event system supporting:
  - Parameter changes
  - Button press/release/hold events
  - Encoder increment/decrement
- **Effect** (`effect.h/cpp`): Base class for audio effects with parameter management
- **EffectParameter** (`effectparameter.h/cpp`): Parameter class with curve support

## Building

Building the project is generally straightforward but there are some things to remember:
- The program won't fit into the 128kB of ARM flash, so the program is slated to boot from the external 8MB QSPI flash
- The QSPI flash needs to be programmed by the Daisy bootloader, not the default DFU bootloader
- You have to program the Daisy ARM flash with the Daisy bootloader with the command "make program-boot"
- When the Daisy bootloader resets, you have to press the "boot" button to allow it to be programmed
- If you don't press the "boot" button within 3s, the bootloader boots the program in QSPI flash

### Prerequisites

- ARM GCC toolchain
- libDaisy library
- DaisySP library
- Make

### Build Commands

```bash
# Build project
make

# Clean and build
make clean && make

# Build and program via ST-Link
make program

# Build and program via DFU
make program-dfu
```

### VS Code Tasks

- `build`: Clean and build the project
- `build_and_program`: Build and program via ST-Link
- `build_and_program_dfu`: Build and program via DFU
- `build_all`: Build with dependencies (libDaisy + DaisySP)

## File Structure

```
├── Perspective.cpp          # Main application entry point
├── hardware.h/cpp          # Hardware abstraction layer
├── ui.h/cpp                # User interface manager
├── uieventhandler.h/cpp    # Event handling system
├── effect.h/cpp            # Base effect class
├── effectparameter.h/cpp   # Parameter system with curves
├── nopullswitch.h/cpp      # Custom switch with NOPULL
├── nopullencoder.h/cpp     # Custom encoder with NOPULL
├── hardware_pins.h         # Pin definitions
└── Makefile                # Build configuration
```

## Event Types

- `PARAMETER_CHANGED`: Potentiometer or encoder value changed
- `BUTTON_PRESSED`: Button pressed
- `BUTTON_RELEASED`: Button released
- `BUTTON_HELD`: Button held for 2+ seconds
- `ENCODER_INCREMENT`: Encoder turned clockwise
- `ENCODER_DECREMENT`: Encoder turned counter-clockwise

## Parameter Curves

- `LINEAR`: Direct linear mapping
- `LOGARITHMIC`: Logarithmic response
- `REVERSE_LOGARITHMIC`: Inverse logarithmic response
- `A_TAPER`: Audio taper (standard potentiometer curve)
- `W_TAPER`: W-taper (anti-log)
- `EXPONENTIAL`: Exponential mapping
- `SQUARED`: Squared curve
- `CUBED`: Cubic curve

## License

[Add your license here]
