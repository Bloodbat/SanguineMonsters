#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

struct Alembic : SanguineModule {

	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_GAIN_CV, PORT_MAX_CHANNELS),
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_CHANNEL, PORT_MAX_CHANNELS),
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_MASTER_MODULE,
		LIGHTS_COUNT
	};

	Alembic();

	void onExpanderChange(const ExpanderChangeEvent& e) override;
};