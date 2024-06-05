#pragma once

#include "sanguinecomponents.hpp"

struct SeqStepSwitch :SanguineLightUpSwitch {
	SeqStepSwitch() {
		addHalo(nvgRGB(0, 207, 76));
		addHalo(nvgRGB(207, 38, 0));
		momentary = false;
	}
};

struct SeqControlSwitch :SanguineLightUpSwitch {
	SeqControlSwitch() {
		addHalo(nvgRGB(0, 167, 255));
		addHalo(nvgRGB(239, 250, 110));
	}
};
