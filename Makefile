FLAGS += -I./pcg

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard pcg/*.c)

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk