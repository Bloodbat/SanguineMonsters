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
		ENUMS(LIGHT_EYE_1, 3),
		ENUMS(LIGHT_EYE_2, 3),
		LIGHT_GAIN,
		LIGHT_FOLD,
		LIGHTS_COUNT
	};

	float fold;
	float gain;

	dsp::ClockDivider lightDivider;

	const int kLightFrequency = 64;

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

		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		bool leftInConnected = inputs[INPUT_LEFT].isConnected();
		bool rightInConnected = inputs[INPUT_RIGHT].isConnected();
		bool normalled = true;

		float voltageLeft = 0.f;
		float voltageRight = 0.f;
		float voltageSumLeft = 0.f;
		float voltageSumRight = 0.f;

		bool lightsTurn = lightDivider.process();
		const float sampleTime = args.sampleTime * kLightFrequency;

		int channelCount = std::max(inputs[INPUT_LEFT].getChannels(), inputs[INPUT_RIGHT].getChannels());

		if (channelCount > 0) {
			fold = clamp(params[PARAM_FOLD].getValue() + inputs[INPUT_FOLD].getVoltage(), 0.f, 10.f);
			gain = clamp(params[PARAM_GAIN].getValue() + inputs[INPUT_GAIN].getVoltage(), 0.f, 20.f);

			for (int channel = 0; channel < channelCount; channel++) {
				if (leftInConnected) {
					float newVoltage = inputs[INPUT_LEFT].getVoltage(channel) * gain;
					voltageLeft = newVoltage;
					voltageRight = newVoltage;
					outputs[OUTPUT_LEFT].setChannels(channelCount);
					outputs[OUTPUT_RIGHT].setChannels(channelCount);
				}
				if (rightInConnected) {
					normalled = false;
					voltageRight = inputs[INPUT_RIGHT].getVoltage(channel) * gain;
				}

				// Distortion
				if (leftInConnected) {
					doDistortion(voltageLeft, OUTPUT_LEFT, channel, voltageSumLeft, voltageSumRight, normalled, OUTPUT_RIGHT);
				}

				if (rightInConnected) {
					doDistortion(voltageRight, OUTPUT_RIGHT, channel, voltageSumRight, voltageSumRight);
				}
			}

			if (lightsTurn) {
				if (channelCount < 2) {
					lights[LIGHT_EYE_1 + 0].setBrightnessSmooth(math::rescale(voltageSumLeft, 0.f, 5.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_EYE_1 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_1 + 2].setBrightnessSmooth(0.f, sampleTime);
					if (normalled) {
						lights[LIGHT_EYE_2].setBrightnessSmooth(math::rescale(voltageSumLeft, 0.f, 5.f, 0.f, 1.f), sampleTime);
						lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(0.f, sampleTime);
					}
					else {
						lights[LIGHT_EYE_2].setBrightnessSmooth(math::rescale(voltageSumRight, 0.f, 5.f, 0.f, 1.f), sampleTime);
						lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(0.f, sampleTime);
					}
				}
				else {
					float rescaled = math::rescale(voltageSumLeft / channelCount, 0.f, 5.f, 0.f, 1.f);
					lights[LIGHT_EYE_1 + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_1 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_1 + 2].setBrightnessSmooth(rescaled, sampleTime);
					if (normalled) {
						lights[LIGHT_EYE_2].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(rescaled, sampleTime);
					}
					else {
						rescaled = math::rescale(voltageSumRight / channelCount, 0.f, 5.f, 0.f, 1.f);
						lights[LIGHT_EYE_2].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(rescaled, sampleTime);
					}
				}
				lights[LIGHT_GAIN].setBrightnessSmooth(math::rescale(gain, 0.f, 20.f, 0.f, 1.f), sampleTime);
				lights[LIGHT_FOLD].setBrightnessSmooth(math::rescale(fold, 0.f, 10.f, 0.f, 1.f), sampleTime);
			}
		}
	}

	inline void doDistortion(float inVoltage, int mainOutput, int channelNum, float& voltageSum1, float& voltageSum2, bool normalOutput = false, int altOutput = -1) {
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
		voltageSum1 += inVoltage;

		if (outputs[mainOutput].isConnected()) {
			outputs[mainOutput].setVoltage(inVoltage, channelNum);
		}

		if (normalOutput) {
			if (outputs[altOutput].isConnected()) {
				outputs[altOutput].setVoltage(inVoltage, channelNum);
			}
			voltageSum2 += inVoltage;
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

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(22.879, 39.583)), module, Werewolf::LIGHT_EYE_1));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(38.602, 39.583)), module, Werewolf::LIGHT_EYE_2));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(8.947, 83.56)), module, Werewolf::PARAM_GAIN));

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(8.947, 90.978)), module, Werewolf::LIGHT_GAIN));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(51.908, 90.978)), module, Werewolf::LIGHT_FOLD));

		addParam(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(51.908, 83.56)), module, Werewolf::PARAM_FOLD));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(23.04, 98.047)), module, Werewolf::INPUT_GAIN));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(37.814, 98.047)), module, Werewolf::INPUT_FOLD));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(9.866, 112.894)), module, Werewolf::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(21.771, 112.894)), module, Werewolf::INPUT_RIGHT));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(39.083, 112.894)), module, Werewolf::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(50.988, 112.894)), module, Werewolf::OUTPUT_RIGHT));

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight();
		inLight->box.pos = mm2px(Vec(12.525, 104.387));
		inLight->module = module;
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight();
		outLight->box.pos = mm2px(Vec(41.742, 103.987));
		outLight->module = module;
		addChild(outLight);
	}
};

Model* modelWerewolf = createModel<Werewolf, WerewolfWidget>("Sanguine-Werewolf");