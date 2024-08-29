#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"

struct Aion : Module {

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
		LIGHTS_COUNT
	};

	bool timerStarted[4];

	bool lastTimerEdge[4];

	int setTimerValues[4];
	int currentTimerValues[4];

	const int kKnobsFrequency = 64;

	float currentTime = 0.f;

	dsp::ClockDivider knobsDivider;

	dsp::BooleanTrigger btResetButtons[4];
	dsp::BooleanTrigger btRunButtons[4];
	dsp::BooleanTrigger btTriggerButtons[4];
	dsp::SchmittTrigger stResetInputs[4];
	dsp::SchmittTrigger stRunInputs[4];
	dsp::SchmittTrigger stTriggerInputs[4];
	dsp::PulseGenerator pgTimerLights[4];
	dsp::PulseGenerator pgTriggerOutputs[4];

	Aion() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < 4; i++) {
			int currentNumber = i + 1;

			configParam(PARAM_TIMER_1 + i, 1.f, 99.f, 1.f, string::f("Timer %d", currentNumber));
			paramQuantities[PARAM_TIMER_1 + i]->snapEnabled = true;

			configButton(PARAM_RESTART_1 + i, string::f("Auto restart timer %d on timer end", currentNumber));
			configButton(PARAM_START_1 + i, string::f("Start/stop timer %d", currentNumber));
			configButton(PARAM_TRIGGER_1 + i, string::f("Advance timer %d", currentNumber));
			configButton(PARAM_RESET_1 + i, string::f("Reset timer %d", currentNumber));

			configInput(INPUT_TRIGGER_1 + i, string::f("Advance timer %d", currentNumber));
			configInput(INPUT_RESET_1 + i, string::f("Reset timer %d", currentNumber));
			configInput(INPUT_RUN_1 + i, string::f("Start/stop timer %d", currentNumber));

			configOutput(OUTPUT_TRIGGER_1 + i, string::f("Timer end %d", currentNumber));

			setTimerValues[i] = 1;
			currentTimerValues[i] = 1;
			timerStarted[i] = false;
			lastTimerEdge[i] = false;

			knobsDivider.setDivision(kKnobsFrequency);
		}
	}

	void process(const ProcessArgs& args) override {

		bool controlsTurn = knobsDivider.process();

		bool internalTimerSecond = false;
		currentTime += args.sampleTime;
		if (currentTime > 1.f) {
			currentTime -= 1.f;
			internalTimerSecond = true;
		}
		else if (currentTime < 0.5f) {
			internalTimerSecond = true;
		}

		for (int i = 0; i < 4; i++) {
			if (internalTimerSecond) {
				if (!inputs[INPUT_TRIGGER_1 + i].isConnected()) {

					if (lastTimerEdge[i] != internalTimerSecond) {
						if (timerStarted[i]) {
							decreaseTimer(i);
						}
						lastTimerEdge[i] = internalTimerSecond;
					}

					lights[LIGHT_TIMER_1 + i].setBrightnessSmooth(1.f, args.sampleTime);
				}
			}
			else {
				if (!inputs[INPUT_TRIGGER_1 + i].isConnected()) {
					lights[LIGHT_TIMER_1 + i].setBrightnessSmooth(0.f, args.sampleTime);
				}
				lastTimerEdge[i] = internalTimerSecond;
			}

			if (stResetInputs[i].process(inputs[INPUT_RESET_1 + i].getVoltage())) {
				currentTimerValues[i] = setTimerValues[i];
			}

			if (stRunInputs[i].process(inputs[INPUT_RUN_1 + i].getVoltage()) && currentTimerValues[i] > 0) {
				timerStarted[i] = !timerStarted[i];
			}

			if (stTriggerInputs[i].process(inputs[INPUT_TRIGGER_1 + i].getVoltage()) && timerStarted[i]) {
				pgTimerLights[i].trigger(0.05f);
				decreaseTimer(i);
			}

			if (controlsTurn) {
				if (btResetButtons[i].process(params[PARAM_RESET_1 + i].getValue())) {
					currentTimerValues[i] = setTimerValues[i];
				}

				if (btRunButtons[i].process(params[PARAM_START_1 + i].getValue()) && currentTimerValues[i] > 0) {
					timerStarted[i] = !timerStarted[i];
				}

				if (btTriggerButtons[i].process(params[PARAM_TRIGGER_1 + i].getValue()) && timerStarted[i]) {
					pgTimerLights[i].trigger(0.05f);
					decreaseTimer(i);
				}

				if (setTimerValues[i] != int(params[PARAM_TIMER_1 + i].getValue())) {
					setTimerValues[i] = params[PARAM_TIMER_1 + i].getValue();
					currentTimerValues[i] = params[PARAM_TIMER_1 + i].getValue();
				}
			}

			if (outputs[OUTPUT_TRIGGER_1 + i].isConnected()) {
				outputs[OUTPUT_TRIGGER_1 + i].setVoltage(pgTriggerOutputs[i].process(args.sampleTime) ? 10.f : 0.f);
			}

			if (inputs[INPUT_TRIGGER_1 + i].isConnected()) {
				lights[LIGHT_TIMER_1 + i].setBrightnessSmooth(pgTimerLights[i].process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
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
				timerStarted[timerNum] = false;
				currentTimerValues[timerNum] = 0;
			}
			else {
				currentTimerValues[timerNum] = setTimerValues[timerNum];
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* timersStartedJ = json_array();
		for (int i = 0; i < 4; i++) {
			json_t* timerJ = json_boolean(timerStarted[i]);
			json_array_append_new(timersStartedJ, timerJ);
		}
		json_object_set_new(rootJ, "timersStarted", timersStartedJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* timersStartedJ = json_object_get(rootJ, "timersStarted");
		size_t idx;
		json_t* timerJ;
		json_array_foreach(timersStartedJ, idx, timerJ) {
			timerStarted[idx] = json_boolean_value(timerJ);
		}
	}
};

struct AionWidget : ModuleWidget {
	AionWidget(Aion* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_22hp_purple.svg", "res/aion.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* aionFramebuffer = new FramebufferWidget();
		addChild(aionFramebuffer);

		// Timer 1

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(9.3, 26.424), module, Aion::LIGHT_TIMER_1));

		SanguineTinyNumericDisplay* displayTimer1 = new SanguineTinyNumericDisplay(2, module, 21.675, 27.047);
		aionFramebuffer->addChild(displayTimer1);
		displayTimer1->fallbackNumber = 1;

		if (module)
			displayTimer1->values.numberValue = &module->currentTimerValues[0];

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.411, 27.047), module, Aion::PARAM_TIMER_1));

		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(45.87, 14.631), module, Aion::PARAM_RESTART_1));

		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(6.75, 40.397), module, Aion::PARAM_START_1));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 51.545), module, Aion::INPUT_RUN_1));

		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(20.446, 40.397), module, Aion::PARAM_TRIGGER_1));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 51.545), module, Aion::INPUT_TRIGGER_1));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(34.161, 40.397), module, Aion::PARAM_RESET_1));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.161, 51.545), module, Aion::INPUT_RESET_1));

		SanguineStaticRGBLight* lightOutput1 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 47.87, 44.546, true, kSanguineYellowLight);
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 51.545), module, Aion::OUTPUT_TRIGGER_1));

		// Timer 2

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(65.065, 27.047), module, Aion::LIGHT_TIMER_2));

		SanguineTinyNumericDisplay* displayTimer2 = new SanguineTinyNumericDisplay(2, module, 77.441, 27.047);
		aionFramebuffer->addChild(displayTimer2);
		displayTimer2->fallbackNumber = 1;

		if (module)
			displayTimer2->values.numberValue = &module->currentTimerValues[1];

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(94.176, 27.047), module, Aion::PARAM_TIMER_2));

		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(101.635, 14.631), module, Aion::PARAM_RESTART_2));

		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(62.515, 40.397), module, Aion::PARAM_START_2));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(62.515, 51.545), module, Aion::INPUT_RUN_2));

		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(76.211, 40.397), module, Aion::PARAM_TRIGGER_2));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(76.211, 51.545), module, Aion::INPUT_TRIGGER_2));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(90.026, 40.397), module, Aion::PARAM_RESET_2));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(90.026, 51.545), module, Aion::INPUT_RESET_2));

		SanguineStaticRGBLight* lightOutput2 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 103.635, 44.546, true, kSanguineYellowLight);
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(103.635, 51.545), module, Aion::OUTPUT_TRIGGER_2));

		// Timer 3

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(9.3, 73.246), module, Aion::LIGHT_TIMER_3));

		SanguineTinyNumericDisplay* displayTimer3 = new SanguineTinyNumericDisplay(2, module, 21.675, 73.246);
		aionFramebuffer->addChild(displayTimer3);
		displayTimer3->fallbackNumber = 1;

		if (module)
			displayTimer3->values.numberValue = &module->currentTimerValues[2];

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.411, 73.246), module, Aion::PARAM_TIMER_3));

		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(45.87, 60.833), module, Aion::PARAM_RESTART_3));

		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(6.75, 86.6), module, Aion::PARAM_START_3));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 97.748), module, Aion::INPUT_RUN_3));

		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(20.446, 86.6), module, Aion::PARAM_TRIGGER_3));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 97.748), module, Aion::INPUT_TRIGGER_3));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(34.261, 86.6), module, Aion::PARAM_RESET_3));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.261, 97.748), module, Aion::INPUT_RESET_3));

		SanguineStaticRGBLight* lightOutput3 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 47.87, 90.749, true, kSanguineYellowLight);
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 97.748), module, Aion::OUTPUT_TRIGGER_3));

		// Timer 4

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(65.065, 73.246), module, Aion::LIGHT_TIMER_4));

		SanguineTinyNumericDisplay* displayTimer4 = new SanguineTinyNumericDisplay(2, module, 77.441, 73.246);
		aionFramebuffer->addChild(displayTimer4);
		displayTimer4->fallbackNumber = 1;

		if (module)
			displayTimer4->values.numberValue = &module->currentTimerValues[3];

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(94.176, 73.246), module, Aion::PARAM_TIMER_4));

		addParam(createParam<SeqButtonRestartSmall>(millimetersToPixelsVec(101.635, 60.833), module, Aion::PARAM_RESTART_4));

		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(62.515, 86.6), module, Aion::PARAM_START_4));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(62.515, 97.748), module, Aion::INPUT_RUN_4));

		addParam(createParamCentered<SeqButtonClock>(millimetersToPixelsVec(76.211, 86.6), module, Aion::PARAM_TRIGGER_4));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(76.211, 97.748), module, Aion::INPUT_TRIGGER_4));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(90.026, 86.6), module, Aion::PARAM_RESET_4));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(90.026, 97.748), module, Aion::INPUT_RESET_4));

		SanguineStaticRGBLight* lightOutput4 = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 103.635, 90.749, true, kSanguineYellowLight);
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(103.635, 97.748), module, Aion::OUTPUT_TRIGGER_4));

		// Sanguine logo lights

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 46.116, 113.48);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 59.248, 120.435);
		addChild(monstersLight);
	}
};

Model* modelAion = createModel<Aion, AionWidget>("Sanguine-Aion");