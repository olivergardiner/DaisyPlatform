# Project Name
TARGET = Perspective

# Sources
CPP_SOURCES = application.cpp hardware.cpp effectparameter.cpp effect.cpp compoundeffect.cpp ui/uieventhandler.cpp ui/ui.cpp perspective.cpp nopullcontrols.cpp effects/choruseffect.cpp effects/delayeffect.cpp effects/flangereffect.cpp effects/waheffect.cpp effects/bandpasseffect.cpp effects/autowaheffect.cpp 

# Library Locations
LIBDAISY_DIR ?= dependencies/libDaisy
DAISYSP_DIR ?= dependencies/DaisySP
DAISYGFX2_DIR ?= dependencies/DaisySeedGFX2
CYCFI_Q_DIR ?= dependencies/cycfi/q/q_lib

USE_DAISYSP_LGPL=1

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

C_INCLUDES += -I$(DAISYSP_DIR)/src
C_INCLUDES += -I$(DAISYGFX2_DIR)
C_INCLUDES += -I$(CYCFI_Q_DIR)/include
