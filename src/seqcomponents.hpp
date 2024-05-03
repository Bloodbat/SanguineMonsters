#pragma once

#include "sanguinecomponents.hpp"

struct SeqStepSwitch :SanguineLightUpSwitch {
	SeqStepSwitch() {
		haloColorOn = nvgRGB(207, 38, 0);
		haloColorOff = nvgRGB(0, 207, 76);
	}
};

struct SeqControlSwitch :SanguineLightUpSwitch {
	SeqControlSwitch() {
		haloColorOn = nvgRGB(239, 250, 110);
		haloColorOff = nvgRGB(0, 167, 255);
	}
};
