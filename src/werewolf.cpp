#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

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

		float voltageInLeft = 0.f;
		float voltageInRight = 0.f;
		float voltageOutLeft = 0.f;
		float voltageOutRight = 0.f;
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
					voltageInLeft = newVoltage;
					voltageInRight = newVoltage;
					outputs[OUTPUT_LEFT].setChannels(channelCount);
					outputs[OUTPUT_RIGHT].setChannels(channelCount);
				}
				if (rightInConnected) {
					normalled = false;
					voltageInRight = inputs[INPUT_RIGHT].getVoltage(channel) * gain;
				}

				// Distortion
				if (leftInConnected) {
					doDistortion(voltageInLeft, voltageOutLeft);
				}

				if (rightInConnected) {
					doDistortion(voltageInRight, voltageOutRight);
				}

				voltageSumLeft += voltageOutLeft;

				if (outputs[OUTPUT_LEFT].isConnected()) {
					outputs[OUTPUT_LEFT].setVoltage(voltageOutLeft, channel);
				}

				if (normalled) {
					if (outputs[OUTPUT_RIGHT].isConnected()) {
						outputs[OUTPUT_RIGHT].setVoltage(voltageOutLeft, channel);
					}
					voltageSumRight += voltageOutLeft;
				}
				else {
					if (outputs[OUTPUT_RIGHT].isConnected()) {
						outputs[OUTPUT_RIGHT].setVoltage(voltageOutRight, channel);
					}
					voltageSumRight += voltageOutRight;
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

	inline void doDistortion(float inVoltage, float& outVoltage) {
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
		outVoltage = inVoltage;
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

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(22.879, 39.583), module, Werewolf::LIGHT_EYE_1));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.602, 39.583), module, Werewolf::LIGHT_EYE_2));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(8.947, 83.56), module, Werewolf::PARAM_GAIN));

		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(8.947, 90.978), module, Werewolf::LIGHT_GAIN));
		addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(51.908, 90.978), module, Werewolf::LIGHT_FOLD));

		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(51.908, 83.56), module, Werewolf::PARAM_FOLD));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(23.04, 98.047), module, Werewolf::INPUT_GAIN));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(37.814, 98.047), module, Werewolf::INPUT_FOLD));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(9.866, 112.894), module, Werewolf::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(21.771, 112.894), module, Werewolf::INPUT_RIGHT));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(39.083, 112.894), module, Werewolf::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(50.988, 112.894), module, Werewolf::OUTPUT_RIGHT));

		SanguinePolyInputLight* inLight = createWidgetCentered<SanguinePolyInputLight>(millimetersToPixelsVec(15.819, 106.451));
		inLight->module = module;
		addChild(inLight);

		SanguinePolyOutputLight* outLight = createWidgetCentered<SanguinePolyOutputLight>(millimetersToPixelsVec(45.036, 106.051));
		outLight->module = module;
		addChild(outLight);
	}
};

Model* modelWerewolf = createModel<Werewolf, WerewolfWidget>("Sanguine-Werewolf");