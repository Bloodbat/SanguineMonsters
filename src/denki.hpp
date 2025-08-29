#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "kitsunecommon.hpp"

struct Denki : SanguineModule {

	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_GAIN_CV, kitsune::kMaxSections),
		ENUMS(INPUT_OFFSET_CV, kitsune::kMaxSections),
		INPUTS_COUNT
	};


	enum OutputIds {
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_MASTER_MODULE,
		ENUMS(LIGHT_GAIN_CV, kitsune::kMaxSections * 3),
		ENUMS(LIGHT_OFFSET_CV, kitsune::kMaxSections * 3),
		LIGHTS_COUNT
	};

	Denki();

	bool getGainConnected(const int channel) const;
	bool getOffsetConnected(const int channel) const;

	void setGainConnected(const int channel, const bool value);
	void setOffsetConnected(const int channel, const bool value);

	void onExpanderChange(const ExpanderChangeEvent& e) override;

	void onPortChange(const PortChangeEvent& e) override;

private:
	bool gainsConnected[kitsune::kMaxSections];
	bool offsetsConnected[kitsune::kMaxSections];
};
