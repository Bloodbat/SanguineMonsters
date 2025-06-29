#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#ifndef METAMODULE
#include "seqcomponents.hpp"
#endif

struct Aion : SanguineModule {

	enum ParamIds {
		PARAM_TIMER_1,
		PARAM_TIMER_2,
		PARAM_TIMER_3,
		PARAM_TIMER_4,
		PARAM_RESTART_1,
		PARAM_RESTART_2,
		PARAM_RESTART_3,
		PARAM_RESTART_4,
		PARAM_START_1,
		PARAM_START_2,
		PARAM_START_3,
		PARAM_START_4,
		PARAM_TRIGGER_1,
		PARAM_TRIGGER_2,
		PARAM_TRIGGER_3,
		PARAM_TRIGGER_4,
		PARAM_RESET_1,
		PARAM_RESET_2,
		PARAM_RESET_3,
		PARAM_RESET_4,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_TRIGGER_1,
		INPUT_TRIGGER_2,
		INPUT_TRIGGER_3,
		INPUT_TRIGGER_4,
		INPUT_RESET_1,
		INPUT_RESET_2,
		INPUT_RESET_3,
		INPUT_RESET_4,
		INPUT_RUN_1,
		INPUT_RUN_2,
		INPUT_RUN_3,
		INPUT_RUN_4,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_TRIGGER_1,
		OUTPUT_TRIGGER_2,
		OUTPUT_TRIGGER_3,
		OUTPUT_TRIGGER_4,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_TIMER_1,
		LIGHT_TIMER_2,
		LIGHT_TIMER_3,
		LIGHT_TIMER_4,
#ifdef METAMODULE
		LIGHT_RESTART_1,
		LIGHT_RESTART_2,
		LIGHT_RESTART_3,
		LIGHT_RESTART_4,
#endif
		LIGHTS_COUNT
	};

	static const int kKnobsFrequency = 64;
	static const int kModuleSections = 4;

	bool timersStarted[kModuleSections] = {};

	bool lastTimerEdges[kModuleSections] = {};

	int setTimerValues[kModuleSections] = {};
	int currentTimerValues[kModuleSections] = {};

	float currentTime = 0.f;

	dsp::ClockDivider knobsDivider;

	dsp::BooleanTrigger btResetButtons[kModuleSections];
	dsp::BooleanTrigger btRunButtons[kModuleSections];
	dsp::BooleanTrigger btTriggerButtons[kModuleSections];
	dsp::SchmittTrigger stResetInputs[kModuleSections];
	dsp::SchmittTrigger stRunInputs[kModuleSections];
	dsp::SchmittTrigger stTriggerInputs[kModuleSections];
	dsp::PulseGenerator pgTimerLights[kModuleSections];
	dsp::PulseGenerator pgTriggerOutputs[kModuleSections];

	Aion() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int section = 0; section < kModuleSections; ++section) {
			int currentNumber = section + 1;

			configParam(PARAM_TIMER_1 + section, 1.f, 99.f, 1.f, string::f("Timer %d", currentNumber));
			paramQuantities[PARAM_TIMER_1 + section]->snapEnabled = true;

			configButton(PARAM_RESTART_1 + section, string::f("Auto restart timer %d on timer end", currentNumber));
			configButton(PARAM_START_1 + section, string::f("Start/stop timer %d", currentNumber));
			configButton(PARAM_TRIGGER_1 + section, string::f("Advance timer %d", currentNumber));
			configButton(PARAM_RESET_1 + section, string::f("Reset timer %d", currentNumber));

			configInput(INPUT_TRIGGER_1 + section, string::f("Advance timer %d", currentNumber));
			configInput(INPUT_RESET_1 + section, string::f("Reset timer %d", currentNumber));
			configInput(INPUT_RUN_1 + section, string::f("Start/stop timer %d", currentNumber));

			configOutput(OUTPUT_TRIGGER_1 + section, string::f("Timer end %d", currentNumber));

			setTimerValues[section] = 1;
			currentTimerValues[section] = 1;

			knobsDivider.setDivision(kKnobsFrequency);
		}
	}

	void process(const ProcessArgs& args) override {

		bool bIsControlsTurn = knobsDivider.process();

		bool bInternalTimerSecond = false;
		currentTime += args.sampleTime;
		if (currentTime > 1.f) {
			currentTime -= 1.f;
			bInternalTimerSecond = true;
		} else if (currentTime < 0.5f) {
			bInternalTimerSecond = true;
		}

		for (int section = 0; section < kModuleSections; ++section) {
			if (bInternalTimerSecond) {
				if (!inputs[INPUT_TRIGGER_1 + section].isConnected()) {

					if (lastTimerEdges[section] != bInternalTimerSecond) {
						if (timersStarted[section]) {
							decreaseTimer(section);
						}
						lastTimerEdges[section] = bInternalTimerSecond;
					}

					lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(timersStarted[section] ? 1.f : 0.f, args.sampleTime);
				}
			} else {
				if (!inputs[INPUT_TRIGGER_1 + section].isConnected()) {
					lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(0.f, args.sampleTime);
				}
				lastTimerEdges[section] = bInternalTimerSecond;
			}

			if (stResetInputs[section].process(inputs[INPUT_RESET_1 + section].getVoltage())) {
				currentTimerValues[section] = setTimerValues[section];
			}

			if (stRunInputs[section].process(inputs[INPUT_RUN_1 + section].getVoltage()) && currentTimerValues[section] > 0) {
				timersStarted[section] = !timersStarted[section];
			}

			if (stTriggerInputs[section].process(inputs[INPUT_TRIGGER_1 + section].getVoltage()) && timersStarted[section]) {
				pgTimerLights[section].trigger(0.05f);
				decreaseTimer(section);
			}

			if (bIsControlsTurn) {
				if (btResetButtons[section].process(params[PARAM_RESET_1 + section].getValue())) {
					currentTimerValues[section] = setTimerValues[section];
				}

				if (btRunButtons[section].process(params[PARAM_START_1 + section].getValue()) && currentTimerValues[section] > 0) {
					timersStarted[section] = !timersStarted[section];
				}

				if (btTriggerButtons[section].process(params[PARAM_TRIGGER_1 + section].getValue()) && timersStarted[section]) {
					pgTimerLights[section].trigger(0.05f);
					decreaseTimer(section);
				}

				if (setTimerValues[section] != static_cast<int>(params[PARAM_TIMER_1 + section].getValue())) {
					setTimerValues[section] = params[PARAM_TIMER_1 + section].getValue();
					currentTimerValues[section] = params[PARAM_TIMER_1 + section].getValue();
				}

#ifdef METAMODULE
				lights[LIGHT_RESTART_1 + section].setBrightness(static_cast<bool>(params[PARAM_RESTART_1 + section].getValue()) ?
					kSanguineButtonLightValue : 0.f);
#endif
			}

			if (outputs[OUTPUT_TRIGGER_1 + section].isConnected()) {
				outputs[OUTPUT_TRIGGER_1 + section].setVoltage(pgTriggerOutputs[section].process(args.sampleTime) ? 10.f : 0.f);
			}

			if (inputs[INPUT_TRIGGER_1 + section].isConnected()) {
				lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(pgTimerLights[section].process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
			}
		}
	}

	inline void decreaseTimer(int timerNum) {
		--currentTimerValues[timerNum];

		if (currentTimerValues[timerNum] <= 0) {
			if (outputs[OUTPUT_TRIGGER_1 + timerNum].isConnected()) {
				pgTriggerOutputs[timerNum].trigger();
			}
			if (!params[PARAM_RESTART_1 + timerNum].getValue()) {
				timersStarted[timerNum] = false;
				currentTimerValues[timerNum] = 0;
			} else {
				currentTimerValues[timerNum] = setTimerValues[timerNum];
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* timersStartedJ = json_array();
		for (int section = 0; section < kModuleSections; ++section) {
			json_t* timerJ = json_boolean(timersStarted[section]);
			json_array_append_new(timersStartedJ, timerJ);
		}
		json_object_set_new(rootJ, "timersStarted", timersStartedJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* timersStartedJ = json_object_get(rootJ, "timersStarted");
		size_t idx;
		json_t* timerJ;
		json_array_foreach(timersStartedJ, idx, timerJ) {
			timersStarted[idx] = json_boolean_value(timerJ);
		}
	}
};

struct AionWidget : SanguineModuleWidget {
	explicit AionWidget(Aion* module) {
		setModule(module);

		moduleName = "aion";
		panelSize = SIZE_22;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* aionFramebuffer = new FramebufferWidget();
		addChild(aionFramebuffer);

		// Timer 1
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(6.75, 26.424), module, Aion::LIGHT_TIMER_1));

		SanguineTinyNumericDisplay* displayTimer1 = new SanguineTinyNumericDisplay(2, module, 19.775, 27.047);
		aionFramebuffer->addChild(displayTimer1);
		displayTimer1->fallbackNumber = 1;

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.411, 23.671), module, Aion::PARAM_TIMER_1));

#ifndef METAMODULE
		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(45.87, 14.631), module, Aion::PARAM_RESTART_1));
		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(6.75, 40.397), module, Aion::PARAM_START_1));
		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(20.446, 40.397), module, Aion::PARAM_TRIGGER_1));
		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(34.161, 40.397), module, Aion::PARAM_RESET_1));
		SanguineStaticRGBLight* lightOutput1 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 47.87, 44.546, true, kSanguineYellowLight);
		addChild(lightOutput1);
#else
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(49.77, 16.631), module,
			Aion::PARAM_RESTART_1, Aion::LIGHT_RESTART_1));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(6.75, 42.897), module, Aion::PARAM_START_1));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(20.446, 42.897), module, Aion::PARAM_TRIGGER_1));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(34.161, 42.897), module, Aion::PARAM_RESET_1));
#endif
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 51.545), module, Aion::INPUT_RUN_1));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 51.545), module, Aion::INPUT_TRIGGER_1));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.161, 51.545), module, Aion::INPUT_RESET_1));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 51.545), module, Aion::OUTPUT_TRIGGER_1));

		// Timer 2
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(62.515, 27.047), module, Aion::LIGHT_TIMER_2));

		SanguineTinyNumericDisplay* displayTimer2 = new SanguineTinyNumericDisplay(2, module, 75.54, 27.047);
		aionFramebuffer->addChild(displayTimer2);
		displayTimer2->fallbackNumber = 1;

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(94.176, 23.671), module, Aion::PARAM_TIMER_2));

#ifndef METAMODULE
		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(101.635, 14.631), module, Aion::PARAM_RESTART_2));
		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(62.515, 40.397), module, Aion::PARAM_START_2));
		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(76.211, 40.397), module, Aion::PARAM_TRIGGER_2));
		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(90.026, 40.397), module, Aion::PARAM_RESET_2));
		SanguineStaticRGBLight* lightOutput2 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 103.635, 44.546, true, kSanguineYellowLight);
		addChild(lightOutput2);
#else
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(105.534, 16.631), module,
			Aion::PARAM_RESTART_2, Aion::LIGHT_RESTART_2));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(62.515, 42.897), module, Aion::PARAM_START_2));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(76.211, 42.897), module, Aion::PARAM_TRIGGER_2));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(90.026, 42.897), module, Aion::PARAM_RESET_2));
#endif
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(62.515, 51.545), module, Aion::INPUT_RUN_2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(76.211, 51.545), module, Aion::INPUT_TRIGGER_2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(90.026, 51.545), module, Aion::INPUT_RESET_2));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(103.635, 51.545), module, Aion::OUTPUT_TRIGGER_2));

		// Timer 3
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(6.75, 73.246), module, Aion::LIGHT_TIMER_3));

		SanguineTinyNumericDisplay* displayTimer3 = new SanguineTinyNumericDisplay(2, module, 19.775, 73.246);
		aionFramebuffer->addChild(displayTimer3);
		displayTimer3->fallbackNumber = 1;

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.411, 69.871), module, Aion::PARAM_TIMER_3));

#ifndef METAMODULE
		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(45.87, 60.833), module, Aion::PARAM_RESTART_3));
		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(6.75, 86.6), module, Aion::PARAM_START_3));
		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(20.446, 86.6), module, Aion::PARAM_TRIGGER_3));
		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(34.261, 86.6), module, Aion::PARAM_RESET_3));
		SanguineStaticRGBLight* lightOutput3 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 47.87, 90.749, true, kSanguineYellowLight);
		addChild(lightOutput3);
#else
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(49.77, 62.833), module,
			Aion::PARAM_RESTART_3, Aion::LIGHT_RESTART_3));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(6.75, 89.1), module, Aion::PARAM_START_3));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(20.446, 89.1), module, Aion::PARAM_TRIGGER_3));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(34.261, 89.1), module, Aion::PARAM_RESET_3));
#endif
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 97.748), module, Aion::INPUT_RUN_3));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 97.748), module, Aion::INPUT_TRIGGER_3));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.261, 97.748), module, Aion::INPUT_RESET_3));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 97.748), module, Aion::OUTPUT_TRIGGER_3));

		// Timer 4
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(62.515, 73.246), module, Aion::LIGHT_TIMER_4));

		SanguineTinyNumericDisplay* displayTimer4 = new SanguineTinyNumericDisplay(2, module, 75.54, 73.246);
		aionFramebuffer->addChild(displayTimer4);
		displayTimer4->fallbackNumber = 1;

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(94.176, 69.871), module, Aion::PARAM_TIMER_4));

#ifndef METAMODULE
		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(101.635, 60.833), module, Aion::PARAM_RESTART_4));
		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(62.515, 86.6), module, Aion::PARAM_START_4));
		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(76.211, 86.6), module, Aion::PARAM_TRIGGER_4));
		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(90.026, 86.6), module, Aion::PARAM_RESET_4));
		SanguineStaticRGBLight* lightOutput4 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 103.635, 90.749, true, kSanguineYellowLight);
		addChild(lightOutput4);
#else
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(105.534, 62.833), module,
			Aion::PARAM_RESTART_4, Aion::LIGHT_RESTART_4));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(62.515, 89.1), module, Aion::PARAM_START_4));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(76.211, 89.1), module, Aion::PARAM_TRIGGER_4));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(90.026, 89.1), module, Aion::PARAM_RESET_4));
#endif
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(62.515, 97.748), module, Aion::INPUT_RUN_4));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(76.211, 97.748), module, Aion::INPUT_TRIGGER_4));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(90.026, 97.748), module, Aion::INPUT_RESET_4));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(103.635, 97.748), module, Aion::OUTPUT_TRIGGER_4));

#ifndef METAMODULE
		// Sanguine logo lights
		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 46.116, 113.48);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 59.248, 120.435);
		addChild(monstersLight);
#endif

		if (module) {
			displayTimer1->values.numberValue = &module->currentTimerValues[0];
			displayTimer2->values.numberValue = &module->currentTimerValues[1];
			displayTimer3->values.numberValue = &module->currentTimerValues[2];
			displayTimer4->values.numberValue = &module->currentTimerValues[3];
		}
	}
};

Model* modelAion = createModel<Aion, AionWidget>("Sanguine-Aion");