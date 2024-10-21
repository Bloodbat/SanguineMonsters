FLAGS += -I./pcg

ifndef DEBUGBUILD
EXTRA_FLAGS =
else
EXTRA_FLAGS = -Og
endif

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk