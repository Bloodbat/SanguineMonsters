#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "kitsune.hpp"

struct Denki : SanguineModule {

	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_GAIN_CV, kMaxSections),
		ENUMS(INPUT_OFFSET_CV, kMaxSections),
		INPUTS_COUNT
	};


	enum OutputIds {
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_MASTER_MODULE,
		ENUMS(LIGHT_GAIN_CV, kMaxSections * 3),
		ENUMS(LIGHT_OFFSET_CV, kMaxSections * 3),
		LIGHTS_COUNT
	};

	Denki();

	void onExpanderChange(const ExpanderChangeEvent& e) override;
};
