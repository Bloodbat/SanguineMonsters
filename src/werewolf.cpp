#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct Werewolf : Module {

	enum ParamIds {
		PARAM_GAIN,
		PARAM_FOLD,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_GAIN,
		INPUT_FOLD,
		INPUT_LEFT,
		INPUT_RIGHT,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_LEFT,
		OUTPUT_RIGHT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_EYE_1,
		LIGHT_EYE_2,
		LIGHTS_COUNT
	};

	float voltageLeft = 0.f;
	float voltageRight = 0.f;
	float fold;
	float gain;

	Werewolf() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_GAIN, 0.f, 10.f, 1.f, "Gain");
		configParam(PARAM_FOLD, 0.f, 10.f, 0.f, "Fold");
		configInput(INPUT_LEFT, "Left");
		configInput(INPUT_RIGHT, "Right");
		configInput(INPUT_GAIN, "Gain");
		configInput(INPUT_FOLD, "Fold");
		configOutput(OUTPUT_LEFT, "Left");
		configOutput(OUTPUT_RIGHT, "Right");

		configBypass(INPUT_LEFT, OUTPUT_LEFT);
		configBypass(INPUT_RIGHT, OUTPUT_RIGHT);
	}

	void process(const ProcessArgs& args) override {
		bool leftInConnected = inputs[INPUT_LEFT].isConnected();
		bool rightInConnected = inputs[INPUT_RIGHT].isConnected();

		fold = params[PARAM_FOLD].getValue() + clamp(inputs[INPUT_FOLD].getNormalVoltage(0.f), 0.f, 10.f);
		gain = params[PARAM_GAIN].getValue() + clamp(inputs[INPUT_GAIN].getNormalVoltage(0.f), 0.f, 10.f);

		if (leftInConnected) {
			voltageLeft = inputs[INPUT_LEFT].getVoltage() * gain;
			voltageRight = inputs[INPUT_RIGHT].getVoltage() * gain;
		}
		if (rightInConnected) {
			voltageRight = inputs[INPUT_RIGHT].getVoltage() * gain;
		}

		// Distortion
		if (leftInConnected) {
			doDistortion(voltageLeft, OUTPUT_LEFT, LIGHT_EYE_1, true, INPUT_RIGHT, OUTPUT_RIGHT, LIGHT_EYE_2);
		}

		if (rightInConnected) {
			doDistortion(voltageRight, OUTPUT_RIGHT, LIGHT_EYE_2);
		}
	}

	inline void doDistortion(float inVoltage, int mainOutput, int light, bool normalOutput = false, int altInput = -1, int altOutput = -1, int altLight = -1) {
		for (int i = 0; i < 100; i++) {
			if (inVoltage < -5.f) {
				inVoltage = -5.f + (-inVoltage - 5.f) * fold / 5.f;
			}
			if (inVoltage > 5.f) {
				inVoltage = 5.f - (inVoltage - 5.f) * fold / 5.f;
			}

			if (inVoltage >= -5.f && inVoltage <= 5.f) {
				break;
			}

			if (i == 99) {
				inVoltage = 0.f;
			}
		}
		if (outputs[mainOutput].isConnected()) {
			outputs[mainOutput].setVoltage(inVoltage);
		}
		lights[light].setBrightness(math::rescale(inVoltage, 0.f, 5.f, 0.f, 1.f));

		if (normalOutput && !inputs[altInput].isConnected()) {
			if (outputs[altOutput].isConnected()) {
				outputs[OUTPUT_RIGHT].setVoltage(inVoltage);
			}
			lights[altLight].setBrightness(math::rescale(inVoltage, 0.f, 5.f, 0.f, 1.f));
		}
	}
};

struct WerewolfWidget : ModuleWidget {
	WerewolfWidget(Werewolf* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_12hp_purple.svg", "res/werewolf.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(22.879, 39.583)), module, Werewolf::LIGHT_EYE_1));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(38.602, 39.583)), module, Werewolf::LIGHT_EYE_2));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(8.947, 83.56)), module, Werewolf::PARAM_GAIN));
		addParam(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(51.908, 83.56)), module, Werewolf::PARAM_FOLD));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(23.04, 98.047)), module, Werewolf::INPUT_GAIN));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(37.814, 98.047)), module, Werewolf::INPUT_FOLD));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(9.866, 112.894)), module, Werewolf::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(21.771, 112.894)), module, Werewolf::INPUT_RIGHT));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(39.083, 112.894)), module, Werewolf::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(50.988, 112.894)), module, Werewolf::OUTPUT_RIGHT));

		SanguineMonoInputLight* inLight = new SanguineMonoInputLight();
		inLight->box.pos = mm2px(Vec(12.525, 104.387));
		inLight->module = module;
		addChild(inLight);

		SanguineMonoOutputLight* outLight = new SanguineMonoOutputLight();
		outLight->box.pos = mm2px(Vec(41.742, 103.987));
		outLight->module = module;
		addChild(outLight);
	}
};

Model* modelWerewolf = createModel<Werewolf, WerewolfWidget>("Sanguine-Werewolf");