#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

static const int kMaxSections = 4;

struct Kitsune : SanguineModule {

	enum ParamIds {
		PARAM_ATTENUATOR1,
		PARAM_ATTENUATOR2,
		PARAM_ATTENUATOR3,
		PARAM_ATTENUATOR4,
		PARAM_OFFSET1,
		PARAM_OFFSET2,
		PARAM_OFFSET3,
		PARAM_OFFSET4,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_VOLTAGE1,
		INPUT_VOLTAGE2,
		INPUT_VOLTAGE3,
		INPUT_VOLTAGE4,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_VOLTAGE1,
		OUTPUT_VOLTAGE2,
		OUTPUT_VOLTAGE3,
		OUTPUT_VOLTAGE4,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_VOLTAGE1, 3),
		ENUMS(LIGHT_VOLTAGE2, 3),
		ENUMS(LIGHT_VOLTAGE3, 3),
		ENUMS(LIGHT_VOLTAGE4, 3),
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;
	const int kLightDivisor = 64;

	Kitsune() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int section = 0; section < kMaxSections; section++) {
			configParam(PARAM_ATTENUATOR1 + section, -1.f, 1.f, 0.f, string::f("Channel %d gain", section + 1), "%", 0, 100);
			configParam(PARAM_OFFSET1 + section, -10.f, 10.f, 0.f, string::f("Channel %d offset", section + 1), " V");
			configOutput(OUTPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));
			configInput(INPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));

			configBypass(INPUT_VOLTAGE1 + section, OUTPUT_VOLTAGE1 + section);

			lightsDivider.setDivision(kLightDivisor);
		}
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		float sampleTime;

		bool isLightsTurn = lightsDivider.process();

		if (isLightsTurn) {
			sampleTime = kLightDivisor * args.sampleTime;
		}

		for (int section = 0; section < kMaxSections; section++) {
			int channelSource = section;

			if ((section == 1 || section == 3) && !inputs[INPUT_VOLTAGE1 + section].isConnected()) {
				channelSource = section - 1;
			}

			int channelCount = inputs[INPUT_VOLTAGE1 + channelSource].getChannels() > 0 ? inputs[INPUT_VOLTAGE1 + channelSource].getChannels() : 1;

			for (int channel = 0; channel < channelCount; channel += 4) {
				float_4 voltages = {};

				voltages = simd::clamp(inputs[INPUT_VOLTAGE1 + channelSource].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR1 + section].getValue() +
					params[PARAM_OFFSET1 + section].getValue(), -10.f, 10.f);

				outputs[OUTPUT_VOLTAGE1 + section].setChannels(channelCount);
				outputs[OUTPUT_VOLTAGE1 + section].setVoltageSimd(voltages, channel);
			}

			if (isLightsTurn) {
				int currentLight = LIGHT_VOLTAGE1 + section * 3;

				if (channelCount == 1) {
					float lightValue = outputs[OUTPUT_VOLTAGE1 + section].getVoltage();
					lights[currentLight + 0].setBrightnessSmooth(math::rescale(-lightValue, 0.f, 5.f, 0.f, 1.f), sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(math::rescale(lightValue, 0.f, 5.f, 0.f, 1.f), sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float* outputVoltages = outputs[OUTPUT_VOLTAGE1 + section].getVoltages();
					float lightValue = 0;
					for (int channel = 0; channel < channelCount; channel++) {
						lightValue += outputVoltages[channel];
					}
					lightValue /= channelCount;
					float redValue = math::rescale(-lightValue, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(lightValue < 0 ? redValue : greenValue, sampleTime);
				}
			}
		}
	}
};

struct KitsuneWidget : SanguineModuleWidget {
	KitsuneWidget(Kitsune* module) {
		setModule(module);

		moduleName = "kitsune";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Section 1
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(12.7, 11.198), module, Kitsune::PARAM_ATTENUATOR1));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 30.085), module, Kitsune::PARAM_OFFSET1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 41.9), module, Kitsune::LIGHT_VOLTAGE1));

		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 5.488, 48.4);
		addChild(lightInput1);

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.488, 55.888), module, Kitsune::INPUT_VOLTAGE1));

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 19.599, 47.999);
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.599, 55.888), module, Kitsune::OUTPUT_VOLTAGE1));

		// Section 2
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(12.7, 73.732), module, Kitsune::PARAM_ATTENUATOR2));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 92.618), module, Kitsune::PARAM_OFFSET2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 104.433), module, Kitsune::LIGHT_VOLTAGE2));

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 5.488, 110.933);
		addChild(lightInput2);

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.488, 118.422), module, Kitsune::INPUT_VOLTAGE2));


		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 19.599, 110.533);
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.599, 118.422), module, Kitsune::OUTPUT_VOLTAGE2));

		// Section 3
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(38.099, 11.198), module, Kitsune::PARAM_ATTENUATOR3));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 30.085), module, Kitsune::PARAM_OFFSET3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 41.9), module, Kitsune::LIGHT_VOLTAGE3));

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 30.887, 48.4);
		addChild(lightInput3);

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(30.887, 55.888), module, Kitsune::INPUT_VOLTAGE3));

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 44.998, 47.999);
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(44.9989, 55.888), module, Kitsune::OUTPUT_VOLTAGE3));

		// Section 4
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.099, 73.732), module, Kitsune::PARAM_ATTENUATOR4));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 92.618), module, Kitsune::PARAM_OFFSET4));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 104.433), module, Kitsune::LIGHT_VOLTAGE4));

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 30.887, 110.933);
		addChild(lightInput4);

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(30.887, 118.422), module, Kitsune::INPUT_VOLTAGE4));

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 44.998, 110.533);
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(44.998, 118.422), module, Kitsune::OUTPUT_VOLTAGE4));
	}
};

Model* modelKitsune = createModel<Kitsune, KitsuneWidget>("Sanguine-Kitsune");