FLAGS += -I./pcgcpp \
  -I./SanguineModulesCommon/src

ifndef DEBUGBUILD
EXTRA_FLAGS =
else
EXTRA_FLAGS = -Og
endif

SOURCES += $(wildcard src/*.cpp)

SOURCES += SanguineModulesCommon/src/sanguinecomponents.cpp
SOURCES += SanguineModulesCommon/src/sanguinehelpers.cpp
SOURCES += SanguineModulesCommon/src/themes.cpp

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
ifdef DEBUGBUILD
FLAGS := $(filter-out -O3,$(FLAGS))
FLAGS := $(filter-out -funsafe-math-optimizations,$(FLAGS))
endif