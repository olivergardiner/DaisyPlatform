# Project Name
TARGET = Perspective
APP_TYPE = BOOT_SRAM

# Sources
CPP_SOURCES = application.cpp hardware.cpp ui/knob.cpp ui/switch.cpp ui/encoder.cpp ui/uieventhandler.cpp ui/ui.cpp perspective.cpp preset.cpp
CPP_SOURCES += effects/effect.cpp effects/tempoeffect.cpp effects/compoundeffect.cpp
CPP_SOURCES += parameters/effectparameter.cpp parameters/potentiometerparameter.cpp parameters/encoderparameter.cpp parameters/toggleparameter.cpp parameters/timeparameter.cpp
CPP_SOURCES += effects/choruseffect.cpp effects/delayeffect.cpp effects/moddelayeffect.cpp effects/flangereffect.cpp effects/reverbeffect.cpp effects/waheffect.cpp
CPP_SOURCES += effects/autowaheffect.cpp effects/autowahv2effect.cpp effects/motorwaheffect.cpp effects/mysteriouseffect.cpp effects/flyeffect.cpp effects/phasereffect.cpp effects/tunereffect.cpp effects/paralleldelayeffect.cpp effects/slapbackdelayeffect.cpp effects/streetseffect.cpp effects/twelvestringeffect.cpp
CPP_SOURCES += dependencies/DaisySeedGFX2/TFT_SPI.cpp dependencies/DaisySeedGFX2/GFX.cpp dependencies/DaisySeedGFX2/cDisplay.cpp

OPT = -O3

# Library Locations
LIBDAISY_DIR ?= dependencies/libDaisy
DAISYSP_DIR ?= dependencies/DaisySP
DAISYSP_LGPL_DIR ?= dependencies/DaisySP/DaisySP-LGPL
DAISYGFX2_DIR ?= dependencies/DaisySeedGFX2
USE_DAISYSP_LGPL=1
LDFLAGS = -u _printf_float

CPP_STANDARD = -std=gnu++20

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Add library paths for DaisySP LGPL
LDFLAGS += -L$(DAISYSP_DIR)/DaisySP-LGPL/build

C_INCLUDES += -I$(DAISYSP_DIR)/src
C_INCLUDES += -I$(DAISYSP_LGPL_DIR)/src
C_INCLUDES += -I$(DAISYGFX2_DIR)

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
