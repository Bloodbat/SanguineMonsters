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
	void onPortChange(const PortChangeEvent& e) override;

	bool getOutputConnected(const int channel) const;
	bool getInputConnected(const int channel) const;

	void setOutputConnected(const int channel, const bool value);
	void setInputConnected(const int channel, const bool value);

private:
	bool bHadMaster = false;
	bool outputsConnected[PORT_MAX_CHANNELS] = {};
	bool inputsConnected[PORT_MAX_CHANNELS] = {};
};