# Project Name
TARGET = Perspective

# Sources
CPP_SOURCES = application.cpp hardware.cpp effectparameter.cpp effect.cpp compoundeffect.cpp ui/knob.cpp ui/switch.cpp ui/encoder.cpp ui/uieventhandler.cpp ui/ui.cpp perspective.cpp nopullcontrols.cpp effects/choruseffect.cpp effects/delayeffect.cpp effects/flangereffect.cpp effects/waheffect.cpp effects/bandpasseffect.cpp effects/autowaheffect.cpp dependencies/DaisySeedGFX2/TFT_SPI.cpp dependencies/DaisySeedGFX2/GFX.cpp dependencies/DaisySeedGFX2/cDisplay.cpp

OPT = -Os

# Library Locations
LIBDAISY_DIR ?= dependencies/libDaisy
DAISYSP_DIR ?= dependencies/DaisySP
DAISYSP_LGPL_DIR ?= dependencies/DaisySP/DaisySP-LGPL
DAISYGFX2_DIR ?= dependencies/DaisySeedGFX2
CYCFI_Q_DIR ?= dependencies/cycfi/q/q_lib
CYCFI_INFRA_DIR ?= dependencies/cycfi/infra

USE_DAISYSP_LGPL=1
LDFLAGS = -u _printf_float

# Set C++ standard to C++20 for cycfi/q library support
CPP_STANDARD = -std=gnu++20

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Add library paths for DaisySP LGPL
LDFLAGS += -L$(DAISYSP_DIR)/DaisySP-LGPL/build

C_INCLUDES += -I$(DAISYSP_DIR)/src
C_INCLUDES += -I$(DAISYSP_LGPL_DIR)/src
C_INCLUDES += -I$(DAISYGFX2_DIR)
C_INCLUDES += -I$(CYCFI_Q_DIR)/include
C_INCLUDES += -I$(CYCFI_INFRA_DIR)/include

# Build dependencies
.PHONY: libdaisy daisysp daisysp-lgpl libs

libdaisy:
	$(MAKE) -C $(LIBDAISY_DIR)

daisysp: libdaisy
	$(MAKE) -C $(DAISYSP_DIR)

daisysp-lgpl: daisysp
	$(MAKE) -C $(DAISYSP_LGPL_DIR)

libs: libdaisy daisysp daisysp-lgpl

# Make sure libraries are built before linking
$(BUILD_DIR)/$(TARGET).elf: libs
