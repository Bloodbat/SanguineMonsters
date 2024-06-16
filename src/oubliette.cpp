#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct Oubliette : Module {

	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_NULL, 16),
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_NULL, 16),
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHTS_COUNT
	};

	Oubliette() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < 16; i++) {
			configInput(INPUT_NULL + i, string::f("Null input %d", i + 1));
			configOutput(OUTPUT_NULL + i, string::f("Null output %d", i + 1));
		}
	}
};

struct OublietteWidget : ModuleWidget {
	OublietteWidget(Oubliette* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/oubliette.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float xBase = 6.012;
		const float xDelta = 9.539;

		const float yInputsBase = 19.648;
		const float yOutputsBase = 85.961;
		const float yDelta = 9.568;

		float currentX = xBase;
		float currentInputsY = yInputsBase;
		float currentOutputsY = yOutputsBase;

		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				addInput(createInputCentered<BananutGreen>(mm2px(Vec(currentX, currentInputsY)), module, Oubliette::INPUT_NULL + x + (y * 4)));
				addOutput(createOutputCentered<BananutRed>(mm2px(Vec(currentX, currentOutputsY)), module, Oubliette::OUTPUT_NULL + x + (y * 4)));
				currentX += xDelta;
			}
			currentX = xBase;
			currentInputsY += yDelta;
			currentOutputsY += yDelta;
		}
	}
};

Model* modelOubliette = createModel<Oubliette, OublietteWidget>("Sanguine-Oubliette");