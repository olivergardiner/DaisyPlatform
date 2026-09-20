# Project Name
TARGET = Perspective
APP_TYPE = BOOT_SRAM

# Sources
CPP_SOURCES = application.cpp hardware.cpp ui/knob.cpp ui/switch.cpp ui/encoder.cpp ui/uieventhandler.cpp ui/ui.cpp perspective.cpp preset.cpp
CPP_SOURCES += effects/effect.cpp effects/tempoeffect.cpp effects/compoundeffect.cpp
CPP_SOURCES += parameters/effectparameter.cpp parameters/valueparameter.cpp parameters/enumparameter.cpp parameters/timeparameter.cpp parameters/integerparameter.cpp
CPP_SOURCES += effects/choruseffect.cpp effects/delayeffect.cpp effects/moddelayeffect.cpp effects/flangereffect.cpp effects/reverbeffect.cpp effects/waheffect.cpp
CPP_SOURCES += effects/autowaheffect.cpp effects/autowahv2effect.cpp effects/motorwaheffect.cpp effects/mysteriouseffect.cpp effects/flyeffect.cpp effects/phasereffect.cpp effects/tremoloeffect.cpp effects/tunereffect.cpp effects/paralleldelayeffect.cpp effects/slapbackdelayeffect.cpp effects/streetseffect.cpp effects/twelvestringeffect.cpp effects/compressoreffect.cpp
CPP_SOURCES += effects/noisegateeffect.cpp effects/driveeffect.cpp effects/tonestackeffect.cpp effects/cabsimeffect.cpp effects/sandmaneffect.cpp
CPP_SOURCES += dependencies/DaisySeedGFX2/TFT_SPI.cpp dependencies/DaisySeedGFX2/GFX.cpp dependencies/DaisySeedGFX2/cDisplay.cpp

OPT = -O3

# Build target: 'stereo' for the stereo effects platform, 'amp' for the mono
# FX + cab sim platform. See platform.h. This is the single point of control:
# platform.h refuses to compile if neither define reaches it.
PLATFORM ?= stereo

ifeq ($(PLATFORM),stereo)
C_DEFS = -DPERSPECTIVE_PLATFORM_STEREO
else ifeq ($(PLATFORM),amp)
C_DEFS = -DPERSPECTIVE_PLATFORM_AMP
else
$(error PLATFORM must be 'stereo' or 'amp', got '$(PLATFORM)')
endif

# Each platform gets its own build directory, so switching PLATFORM cannot
# leave objects from the other one behind.
#
# BUILD_DIR has to arrive as a command-line assignment: the core makefile sets
# it with '=' (which would clobber anything we set before the include) and uses
# it in rule targets, which expand at parse time (so setting it after is too
# late). Only a command-line value beats both. When one has not been given we
# re-invoke make once, supplying it.
ifeq ($(origin BUILD_DIR),undefined)

PERSPECTIVE_GOALS := $(or $(MAKECMDGOALS),all)

.PHONY: $(PERSPECTIVE_GOALS)
$(PERSPECTIVE_GOALS):
	@$(MAKE) --no-print-directory PLATFORM=$(PLATFORM) BUILD_DIR=build-$(PLATFORM) $@

else

# Library Locations
LIBDAISY_DIR ?= dependencies/libDaisy
DAISYSP_DIR ?= dependencies/DaisySP
DAISYSP_LGPL_DIR ?= dependencies/DaisySP/DaisySP-LGPL
DAISYGFX2_DIR ?= dependencies/DaisySeedGFX2
CMSIS_DSP_DIR ?= dependencies/CMSIS-DSP
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
C_INCLUDES += -I$(CMSIS_DSP_DIR)/Include

# Build dependencies
.PHONY: libdaisy daisysp daisysp-lgpl libs

# BUILD_DIR reaches us as a command-line assignment, and make propagates those
# to sub-makes, so the dependencies would try to build into our platform
# directory. Pin each back to its own default.
libdaisy:
	$(MAKE) -C $(LIBDAISY_DIR) BUILD_DIR=build

daisysp: libdaisy
	$(MAKE) -C $(DAISYSP_DIR) BUILD_DIR=build

daisysp-lgpl: daisysp
	$(MAKE) -C $(DAISYSP_LGPL_DIR) BUILD_DIR=build

libs: libdaisy daisysp daisysp-lgpl

# Make sure libraries are built before linking
$(BUILD_DIR)/$(TARGET).elf: libs

endif # BUILD_DIR supplied on the command line
