#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include "kitsunecommon.hpp"
#include "kitsune.hpp"
#ifndef METAMODULE
#include "denki.hpp"
#endif

using simd::float_4;

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
#ifndef METAMODULE
		LIGHT_EXPANDER,
#endif
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;
	const int kLightsFrequency = 64;

	int channelCounts[kitsune::kMaxSections] = {};

#ifndef METAMODULE
	bool bHaveExpander = false;

	Denki* denkiExpander = nullptr;
#endif

	bool inputsConnected[kitsune::kMaxSections] = {};

	kitsune::NormalledModes normalledMode = kitsune::NORMAL_SMART;

	Kitsune() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int section = 0; section < kitsune::kMaxSections; ++section) {
			configParam(PARAM_ATTENUATOR1 + section, -1.f, 1.f, 0.f, string::f("Channel %d gain", section + 1), "%", 0, 100);
			configParam(PARAM_OFFSET1 + section, -10.f, 10.f, 0.f, string::f("Channel %d offset", section + 1), " V");
			configOutput(OUTPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));
			configInput(INPUT_VOLTAGE1 + section, string::f("Channel %d", section + 1));

			configBypass(INPUT_VOLTAGE1 + section, OUTPUT_VOLTAGE1 + section);
		}

		configSwitch(PARAM_NORMALLING_MODE, 0.f, 1.f, 1.f, "Input normalling", kitsune::normallingModes);

		lightsDivider.setDivision(kLightsFrequency);
	}

#ifndef METAMODULE
	void process(const ProcessArgs& args) override {
		bool bIsLightsTurn = lightsDivider.process();

		normalledMode = kitsune::NormalledModes(params[PARAM_NORMALLING_MODE].getValue());

		int channelSources[kitsune::kMaxSections] = { 0, 1, 2, 3 };
		int lastChannelSource = -1;

		if (!bHaveExpander) {
			if (normalledMode == kitsune::NORMAL_SMART) {
				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					assignNormalledInputs(section, channelSources, lastChannelSource);
					processSection(section, channelSources);
				}
			} else {
				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					processSection(section, channelSources);
				}
			}
			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				lights[LIGHT_EXPANDER].setBrightnessSmooth(bHaveExpander * kSanguineButtonLightValue, sampleTime);

				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					setNormalledLights(section, channelSources, sampleTime);

					if (channelCounts[section] == 1) {
						setMonoLights(section, sampleTime);
					} else {
						setPolyLights(section, sampleTime);
					}
				}
			}
		} else {
			if (normalledMode == kitsune::NORMAL_SMART) {
				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					assignNormalledInputs(section, channelSources, lastChannelSource);
					processSectionExpander(section, channelSources);
				}
			} else {
				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					processSectionExpander(section, channelSources);
				}
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				lights[LIGHT_EXPANDER].setBrightnessSmooth(bHaveExpander * kSanguineButtonLightValue, sampleTime);

				for (int section = 0; section < kitsune::kMaxSections; ++section) {
					setNormalledLights(section, channelSources, sampleTime);

					if (channelCounts[section] == 1) {
						setMonoLightsExpander(section, sampleTime);
					} else {
						setPolyLightsExpander(section, sampleTime);
					}
				}
			}
		}
	}
#else
	void process(const ProcessArgs& args) override {
		bool bIsLightsTurn = lightsDivider.process();

		normalledMode = kitsune::NormalledModes(params[PARAM_NORMALLING_MODE].getValue());

		int channelSources[kitsune::kMaxSections] = { 0, 1, 2, 3 };
		int lastChannelSource = -1;

		if (normalledMode == kitsune::NORMAL_SMART) {
			for (int section = 0; section < kitsune::kMaxSections; ++section) {
				assignNormalledInputs(section, channelSources, lastChannelSource);
				processSection(section, channelSources);
			}
		} else {
			for (int section = 0; section < kitsune::kMaxSections; ++section) {
				processSection(section, channelSources);
			}
		}
		if (bIsLightsTurn) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			for (int section = 0; section < kitsune::kMaxSections; ++section) {
				setNormalledLights(section, channelSources, sampleTime);

				if (channelCounts[section] == 1) {
					setMonoLights(section, sampleTime);
				} else {
					setPolyLights(section, sampleTime);
				}
			}
		}
	}
#endif

	void processSection(const int section, int* channelSources) {
		Input* input = &inputs[INPUT_VOLTAGE1 + channelSources[section]];

		int inputChannels = input->getChannels();

		channelCounts[section] = inputChannels > 0 ? inputChannels : 1;

		float gainKnob = params[PARAM_ATTENUATOR1 + section].getValue();
		float offsetKnob = params[PARAM_OFFSET1 + section].getValue();

		int currentOutput = OUTPUT_VOLTAGE1 + section;

		for (int channel = 0; channel < channelCounts[section]; channel += 4) {
			float_4 voltages = input->getVoltageSimd<float_4>(channel);
			float_4 gains = gainKnob;
			float_4 offsets = offsetKnob;

			/* TODO: make manual and module congruent: either we clip it and state so in the manual
			   or we remove the clamp. */
			voltages = simd::clamp(voltages * gains + offsets, -10.f, 10.f);

			outputs[currentOutput].setVoltageSimd(voltages, channel);
		}
		outputs[currentOutput].setChannels(channelCounts[section]);
	}

	void assignNormalledInputs(const int section, int* channelSources, int& lastChannelSource) {
		if (inputsConnected[section]) {
			lastChannelSource = section;
		} else if (lastChannelSource > -1) {
			channelSources[section] = lastChannelSource;
		}
	}

	void setNormalledLights(const int section, int* channelSources, const float& sampleTime) {
		const int currentLight = LIGHT_NORMALLED1 + section * 3;

		RGBLightColor lightColor = kitsune::lightColors[channelSources[section]];
		lights[currentLight].setBrightnessSmooth(lightColor.red, sampleTime);
		lights[currentLight + 1].setBrightnessSmooth(lightColor.green, sampleTime);
		lights[currentLight + 2].setBrightnessSmooth(lightColor.blue, sampleTime);
	}

	void setMonoLights(const int section, const float& sampleTime) {
		const int currentLight = LIGHT_VOLTAGE1 + section * 3;

		float lightValue = outputs[OUTPUT_VOLTAGE1 + section].getVoltage();
		float rescaledLight = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
		lights[currentLight].setBrightnessSmooth(-rescaledLight, sampleTime);
		lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
		lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
	}

	void setPolyLights(const int section, const float& sampleTime) {
		const int currentLight = LIGHT_VOLTAGE1 + section * 3;

		float* outputVoltages = outputs[OUTPUT_VOLTAGE1 + section].getVoltages();
		float lightValue = 0;
		for (int channel = 0; channel < channelCounts[section]; ++channel) {
			lightValue += outputVoltages[channel];
		}
		lightValue /= channelCounts[section];

		float rescaledLight = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
		lights[currentLight].setBrightnessSmooth(-rescaledLight, sampleTime);
		lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
		lights[currentLight + 2].setBrightnessSmooth(lightValue < 0 ? -rescaledLight : rescaledLight, sampleTime);
	}

#ifndef METAMODULE
	void processSectionExpander(const int section, int* channelSources) {
		Input* input = &inputs[INPUT_VOLTAGE1 + channelSources[section]];

		int inputChannels = input->getChannels();

		channelCounts[section] = inputChannels > 0 ? inputChannels : 1;

		float gainKnob = params[PARAM_ATTENUATOR1 + section].getValue();
		float offsetKnob = params[PARAM_OFFSET1 + section].getValue();

		int currentOutput = OUTPUT_VOLTAGE1 + section;

		for (int channel = 0; channel < channelCounts[section]; channel += 4) {
			float_4 voltages = input->getVoltageSimd<float_4>(channel);
			float_4 gains = gainKnob;
			float_4 offsets = offsetKnob;

			applyModulations(section, channel, gains, offsets);

			/* TODO: make manual and module congruent: either we clip it and state so in the manual
			   or we remove the clamp. */
			voltages = simd::clamp(voltages * gains + offsets, -10.f, 10.f);

			outputs[currentOutput].setVoltageSimd(voltages, channel);
		}
		outputs[currentOutput].setChannels(channelCounts[section]);
	}

	void applyModulations(const int section, const int channel, float_4& gains, float_4& offsets) {
		if (denkiExpander->getGainConnected(section)) {
			gains += denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltageSimd<float_4>(channel) / 5.f;
			gains = simd::clamp(gains, -2.f, 2.f);
		}
		if (denkiExpander->getOffsetConnected(section)) {
			offsets += denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltageSimd<float_4>(channel) / 5.f;
			offsets = simd::clamp(offsets, -10.f, 10.f);
		}
	}

	void setMonoLightsExpander(const int section, const float& sampleTime) {
		setMonoLights(section, sampleTime);

		const int currentExpanderGainLight = Denki::LIGHT_GAIN_CV + section * 3;
		float cvGainLightValue = denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltage();
		float rescaledLight = math::rescale(cvGainLightValue, 0.f, 10.f, 0.f, 1.f);
		denkiExpander->getLight(currentExpanderGainLight).setBrightnessSmooth(-rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderGainLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderGainLight + 2).setBrightnessSmooth(0.f, sampleTime);

		const int currentExpanderOffsetLight = Denki::LIGHT_OFFSET_CV + section * 3;
		float cvOffsetLightValue = denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltage();
		rescaledLight = math::rescale(cvOffsetLightValue, 0.f, 10.f, 0.f, 1.f);
		denkiExpander->getLight(currentExpanderOffsetLight).setBrightnessSmooth(-rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderOffsetLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderOffsetLight + 2).setBrightnessSmooth(0.f, sampleTime);
	}

	void setPolyLightsExpander(const int section, const float& sampleTime) {
		const int currentLight = LIGHT_VOLTAGE1 + section * 3;

		float cvGainLightValue = 0.f;
		float cvOffsetLightValue = 0.f;

		float* outputVoltages = outputs[OUTPUT_VOLTAGE1 + section].getVoltages();
		float lightValue = 0;
		for (int channel = 0; channel < channelCounts[section]; ++channel) {
			lightValue += outputVoltages[channel];

			cvGainLightValue += denkiExpander->getInput(Denki::INPUT_GAIN_CV + section).getVoltage(channel);
			cvOffsetLightValue += denkiExpander->getInput(Denki::INPUT_OFFSET_CV + section).getVoltage(channel);
		}
		lightValue /= channelCounts[section];

		float rescaledLight = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
		lights[currentLight].setBrightnessSmooth(-rescaledLight, sampleTime);
		lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
		lights[currentLight + 2].setBrightnessSmooth(lightValue < 0 ? -rescaledLight : rescaledLight, sampleTime);

		const int currentExpanderGainLight = Denki::LIGHT_GAIN_CV + section * 3;

		cvGainLightValue /= channelCounts[section];

		rescaledLight = math::rescale(cvGainLightValue, 0.f, 10.f, 0.f, 1.f);

		denkiExpander->getLight(currentExpanderGainLight).setBrightnessSmooth(-rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderGainLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderGainLight + 2).setBrightnessSmooth(cvGainLightValue < 0 ?
			-rescaledLight : rescaledLight, sampleTime);

		const int currentExpanderOffsetLight = Denki::LIGHT_OFFSET_CV + section * 3;
		cvOffsetLightValue /= channelCounts[section];

		rescaledLight = math::rescale(cvOffsetLightValue, 0.f, 10.f, 0.f, 1.f);

		denkiExpander->getLight(currentExpanderOffsetLight).setBrightnessSmooth(-rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderOffsetLight + 1).setBrightnessSmooth(rescaledLight, sampleTime);
		denkiExpander->getLight(currentExpanderOffsetLight + 2).setBrightnessSmooth(cvOffsetLightValue < 0 ?
			-rescaledLight : rescaledLight, sampleTime);
	}

	void onBypass(const BypassEvent& e) override {
		if (bHaveExpander) {
			denkiExpander->getLight(Denki::LIGHT_MASTER_MODULE).setBrightness(0.f);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHaveExpander) {
			denkiExpander->getLight(Denki::LIGHT_MASTER_MODULE).setBrightness(kSanguineButtonLightValue);
		}
		Module::onUnBypass(e);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		if (e.side == 1) {
			Module* rightModule = getRightExpander().module;
			bHaveExpander = rightModule && rightModule->getModel() == modelDenki &&
				!rightModule->isBypassed();

			if (bHaveExpander) {
				denkiExpander = dynamic_cast<Denki*>(rightModule);
			}
		}
	}
#endif

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT) {
			inputsConnected[e.portId] = e.connecting;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "normalledMode", static_cast<int>(normalledMode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue;

		if (getJsonInt(rootJ, "normalledMode", intValue)) {
			normalledMode = static_cast<kitsune::NormalledModes>(intValue);
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
		panelSize = sanguineThemes::SIZE_10;
		backplateColor = sanguineThemes::PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(48.017, 5.573), module, Kitsune::LIGHT_EXPANDER));
#endif

		// Section 1.
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(12.7, 11.198), module, Kitsune::PARAM_ATTENUATOR1));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 30.085), module, Kitsune::PARAM_OFFSET1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 41.9), module, Kitsune::LIGHT_VOLTAGE1));

#ifndef METAMODULE
		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 5.988, 48.4);
		addChild(lightInput1);

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 19.099, 47.999);
		addChild(lightOutput1);
#endif

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(2.869, 51.176), module, Kitsune::LIGHT_NORMALLED1));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.988, 55.888), module, Kitsune::INPUT_VOLTAGE1));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.099, 55.888), module, Kitsune::OUTPUT_VOLTAGE1));

		// Section 2.
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(12.7, 73.732), module, Kitsune::PARAM_ATTENUATOR2));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 92.618), module, Kitsune::PARAM_OFFSET2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 104.433), module, Kitsune::LIGHT_VOLTAGE2));

#ifndef METAMODULE
		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 5.988, 110.933);
		addChild(lightInput2);

		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 19.099, 110.533);
		addChild(lightOutput2);
#endif

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(2.869, 113.71), module, Kitsune::LIGHT_NORMALLED2));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(5.988, 118.422), module, Kitsune::INPUT_VOLTAGE2));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.099, 118.422), module, Kitsune::OUTPUT_VOLTAGE2));

		// Section 3.
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(38.099, 11.198), module, Kitsune::PARAM_ATTENUATOR3));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 30.085), module, Kitsune::PARAM_OFFSET3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 41.9), module, Kitsune::LIGHT_VOLTAGE3));

#ifndef METAMODULE
		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 31.387, 48.4);
		addChild(lightInput3);

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 44.498, 47.999);
		addChild(lightOutput3);
#endif

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.249, 51.176), module, Kitsune::LIGHT_NORMALLED3));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(31.387, 55.888), module, Kitsune::INPUT_VOLTAGE3));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(44.498, 55.888), module, Kitsune::OUTPUT_VOLTAGE3));

		// Section 4.
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.099, 73.732), module, Kitsune::PARAM_ATTENUATOR4));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 92.618), module, Kitsune::PARAM_OFFSET4));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 104.433), module, Kitsune::LIGHT_VOLTAGE4));

#ifndef METAMODULE
		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 31.387, 110.933);
		addChild(lightInput4);

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 44.498, 110.533);
		addChild(lightOutput4);
#endif

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.249, 113.71), module, Kitsune::LIGHT_NORMALLED4));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(31.387, 118.422), module, Kitsune::INPUT_VOLTAGE4));

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

#ifndef METAMODULE
		menu->addChild(new MenuSeparator);
		const Module* expander = kitsune->rightExpander.module;
		if (expander && expander->model == modelDenki) {
			menu->addChild(createMenuLabel("Denki expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Denki expander", "", [=]() {
				kitsune->addExpander(modelDenki, this);
				}));
		}
#endif
	}
};

Model* modelKitsune = createModel<Kitsune, KitsuneWidget>("Sanguine-Kitsune");