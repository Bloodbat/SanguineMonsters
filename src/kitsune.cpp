#include "sanguinehelpers.hpp"
#include "kitsune.hpp"
#include "denki.hpp"

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
		PARAM_NORMALLING_MODE,
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
		ENUMS(LIGHT_NORMALLED1, 3),
		ENUMS(LIGHT_NORMALLED2, 3),
		ENUMS(LIGHT_NORMALLED3, 3),
		ENUMS(LIGHT_NORMALLED4, 3),
		LIGHT_EXPANDER,
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;
	const int kLightDivisor = 64;

	bool bHasExpander = false;

	kitsune::NormalledModes normalledMode = kitsune::NORMAL_SMART;

	Kitsune() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int section = 0; section < kitsune::kMaxSections; ++section) {
			configParam(PARAM_ATTENUATOR1 + section, -1.f, 1.f, 0.f, string::f("Channel %d gain", section + 1), "%", 0, 100);
			configParam(PARAM_OFFSET1 + section, -10.f, 10.f, 0.f, string::f("Channel %d offset", section + 1), " V");
			configOutput(OUTPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));
			configInput(INPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));

			configBypass(INPUT_VOLTAGE1 + section, OUTPUT_VOLTAGE1 + section);

			lightsDivider.setDivision(kLightDivisor);
		}

		configSwitch(PARAM_NORMALLING_MODE, 0.f, 1.f, 1.f, "Input normalling", kitsune::normallingModes);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		float sampleTime;

		bool bIsLightsTurn = lightsDivider.process();

		Module* denkiExpander = getRightExpander().module;

		bHasExpander = (denkiExpander && denkiExpander->getModel() == modelDenki && !denkiExpander->isBypassed());

		if (bIsLightsTurn) {
			sampleTime = kLightDivisor * args.sampleTime;

			lights[LIGHT_EXPANDER].setBrightnessSmooth(bHasExpander ? kSanguineButtonLightValue : 0.f, sampleTime);
		}

		normalledMode = kitsune::NormalledModes(params[PARAM_NORMALLING_MODE].getValue());

		int channelSources[kitsune::kMaxSections] = { 0, 1, 2, 3 };
		int lastChannelSource = -1;

		for (int section = 0; section < kitsune::kMaxSections; ++section) {
			switch (normalledMode)
			{
			case kitsune::NORMAL_SMART:
				if (inputs[INPUT_VOLTAGE1 + section].isConnected()) {
					lastChannelSource = section;
				} else if (lastChannelSource > -1) {
					channelSources[section] = lastChannelSource;
				}
				break;
			default:
				break;
			}

			int channelCount = inputs[INPUT_VOLTAGE1 + channelSources[section]].getChannels() > 0 ?
				inputs[INPUT_VOLTAGE1 + channelSources[section]].getChannels() : 1;

			for (int channel = 0; channel < channelCount; channel += 4) {
				float_4 voltages = inputs[INPUT_VOLTAGE1 + channelSources[section]].getVoltageSimd<float_4>(channel);
				float_4 gains = params[PARAM_ATTENUATOR1 + section].getValue();
				float_4 offsets = params[PARAM_OFFSET1 + section].getValue();

				if (bHasExpander) {
					if (denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).isConnected()) {
						gains *= denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltageSimd<float_4>(channel) / 5.f;
						gains = simd::clamp(gains, -2.f, 2.f);
					}
					if (denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).isConnected()) {
						offsets += denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltageSimd<float_4>(channel) / 5.f;
						offsets = simd::clamp(offsets, -10.f, 10.f);
					}
				}

				voltages = simd::clamp(voltages * gains + offsets, -10.f, 10.f);

				outputs[OUTPUT_VOLTAGE1 + section].setVoltageSimd(voltages, channel);
			}
			outputs[OUTPUT_VOLTAGE1 + section].setChannels(channelCount);

			if (bIsLightsTurn) {
				int currentLight = LIGHT_NORMALLED1 + section * 3;

				RGBLightColor lightColor = kitsune::lightColors[channelSources[section]];
				lights[currentLight + 0].setBrightnessSmooth(lightColor.red, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(lightColor.green, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(lightColor.blue, sampleTime);

				currentLight = LIGHT_VOLTAGE1 + section * 3;

				if (channelCount == 1) {
					float lightValue = outputs[OUTPUT_VOLTAGE1 + section].getVoltage();
					float rescaledLight = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);

					if (bHasExpander) {
						int currentExpanderGainLight = Denki::LIGHT_GAIN_CV + section * 3;
						float cvGainLightValue = denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltage();
						rescaledLight = math::rescale(cvGainLightValue, 0.f, 10.f, 0.f, 1.f);
						denkiExpander->getLight(currentExpanderGainLight + 0).setBrightnessSmooth(-rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderGainLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderGainLight + 2).setBrightnessSmooth(0.f, sampleTime);

						int currentExpanderOffsetLight = Denki::LIGHT_OFFSET_CV + section * 3;
						float cvOffsetLightValue = denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltage();
						rescaledLight = math::rescale(cvOffsetLightValue, 0.f, 10.f, 0.f, 1.f);
						denkiExpander->getLight(currentExpanderOffsetLight + 0).setBrightnessSmooth(-rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderOffsetLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderOffsetLight + 2).setBrightnessSmooth(0.f, sampleTime);
					}
				} else {
					float* outputVoltages = outputs[OUTPUT_VOLTAGE1 + section].getVoltages();
					float lightValue = 0;
					float cvGainLightValue = 0.f;
					float cvOffsetLightValue = 0.f;
					for (int channel = 0; channel < channelCount; ++channel) {
						lightValue += outputVoltages[channel];

						if (bHasExpander) {
							cvGainLightValue += denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltage(channel);
							cvOffsetLightValue += denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltage(channel);
						}
					}
					lightValue /= channelCount;

					float rescaledLight = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(lightValue < 0 ? -rescaledLight : rescaledLight, sampleTime);

					if (bHasExpander) {
						int currentExpanderGainLight = Denki::LIGHT_GAIN_CV + section * 3;

						cvGainLightValue /= channelCount;

						rescaledLight = math::rescale(cvGainLightValue, 0.f, 10.f, 0.f, 1.f);

						denkiExpander->getLight(currentExpanderGainLight + 0).setBrightnessSmooth(-rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderGainLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderGainLight + 2).setBrightnessSmooth(cvGainLightValue < 0 ? -rescaledLight : rescaledLight, sampleTime);

						int currentExpanderOffsetLight = Denki::LIGHT_OFFSET_CV + section * 3;
						cvOffsetLightValue /= channelCount;

						rescaledLight = math::rescale(cvOffsetLightValue, 0.f, 10.f, 0.f, 1.f);

						denkiExpander->getLight(currentExpanderOffsetLight + 0).setBrightnessSmooth(-rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderOffsetLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
						denkiExpander->getLight(currentExpanderOffsetLight + 2).setBrightnessSmooth(cvOffsetLightValue < 0 ? -rescaledLight : rescaledLight, sampleTime);
					}
				}
			}
		}
	}

	void onBypass(const BypassEvent& e) override {
		if (bHasExpander) {
			Module* denkiExpander = getRightExpander().module;
			denkiExpander->getLight(Denki::LIGHT_MASTER_MODULE).setBrightness(0.f);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHasExpander) {
			Module* denkiExpander = getRightExpander().module;
			denkiExpander->getLight(Denki::LIGHT_MASTER_MODULE).setBrightness(kSanguineButtonLightValue);
		}
		Module::onUnBypass(e);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "normalledMode", json_integer(static_cast<int>(normalledMode)));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* normalledModeJ = json_object_get(rootJ, "normalledMode");

		if (normalledModeJ) {
			normalledMode = static_cast<kitsune::NormalledModes>(json_integer_value(normalledModeJ));
		}
	}

	void setNormalledMode(const kitsune::NormalledModes newMode) {
		params[PARAM_NORMALLING_MODE].setValue(static_cast<float>(newMode));
	}
};

struct KitsuneWidget : SanguineModuleWidget {
	explicit KitsuneWidget(Kitsune* module) {
		setModule(module);

		moduleName = "kitsune";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(48.017, 5.573), module, Kitsune::LIGHT_EXPANDER));

		// Section 1.
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(12.7, 11.198), module, Kitsune::PARAM_ATTENUATOR1));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 30.085), module, Kitsune::PARAM_OFFSET1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 41.9), module, Kitsune::LIGHT_VOLTAGE1));

		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 5.988, 48.4);
		addChild(lightInput1);

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(2.869, 51.176), module, Kitsune::LIGHT_NORMALLED1));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.988, 55.888), module, Kitsune::INPUT_VOLTAGE1));

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 19.099, 47.999);
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.099, 55.888), module, Kitsune::OUTPUT_VOLTAGE1));

		// Section 2.
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(12.7, 73.732), module, Kitsune::PARAM_ATTENUATOR2));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 92.618), module, Kitsune::PARAM_OFFSET2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 104.433), module, Kitsune::LIGHT_VOLTAGE2));

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 5.988, 110.933);
		addChild(lightInput2);

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(2.869, 113.71), module, Kitsune::LIGHT_NORMALLED2));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.988, 118.422), module, Kitsune::INPUT_VOLTAGE2));

		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 19.099, 110.533);
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.099, 118.422), module, Kitsune::OUTPUT_VOLTAGE2));

		// Section 3.
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(38.099, 11.198), module, Kitsune::PARAM_ATTENUATOR3));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 30.085), module, Kitsune::PARAM_OFFSET3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 41.9), module, Kitsune::LIGHT_VOLTAGE3));

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 31.387, 48.4);
		addChild(lightInput3);

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.249, 51.176), module, Kitsune::LIGHT_NORMALLED3));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(31.387, 55.888), module, Kitsune::INPUT_VOLTAGE3));

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 44.498, 47.999);
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(44.498, 55.888), module, Kitsune::OUTPUT_VOLTAGE3));

		// Section 4.
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.099, 73.732), module, Kitsune::PARAM_ATTENUATOR4));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 92.618), module, Kitsune::PARAM_OFFSET4));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 104.433), module, Kitsune::LIGHT_VOLTAGE4));

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 31.387, 110.933);
		addChild(lightInput4);

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.249, 113.71), module, Kitsune::LIGHT_NORMALLED4));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(31.387, 118.422), module, Kitsune::INPUT_VOLTAGE4));

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 44.498, 110.533);
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(44.498, 118.422), module, Kitsune::OUTPUT_VOLTAGE4));

		addParam(createParamCentered<CKSS>(millimetersToPixelsVec(25.4, 102.699), module, Kitsune::PARAM_NORMALLING_MODE));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Kitsune* kitsune = dynamic_cast<Kitsune*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Input normalling", "",
			[=](Menu* menu) {
				menu->addChild(createCheckMenuItem(kitsune::normallingModes[0], "",
					[=]() {return kitsune->normalledMode == kitsune::NORMAL_NONE; },
					[=]() {kitsune->setNormalledMode(kitsune::NORMAL_NONE); }));

				menu->addChild(createCheckMenuItem(kitsune::normallingModes[1], "",
					[=]() {return kitsune->normalledMode == kitsune::NORMAL_SMART; },
					[=]() {kitsune->setNormalledMode(kitsune::NORMAL_SMART); }));
			}
		));

		menu->addChild(new MenuSeparator);
		const Module* expander = kitsune->rightExpander.module;
		if (expander && expander->model == modelDenki) {
			menu->addChild(createMenuLabel("Denki expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Denki expander", "", [=]() {
				kitsune->addExpander(modelDenki, this);
				}));
		}
	}
};

Model* modelKitsune = createModel<Kitsune, KitsuneWidget>("Sanguine-Kitsune");