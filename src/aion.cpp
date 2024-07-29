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

	bool lastTimerEdge = false;

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

			configInput(INPUT_TRIGGER_1 + i, string::f("Trigger timer %d", currentNumber));
			configInput(INPUT_RESET_1 + i, string::f("Reset timer %d", currentNumber));
			configInput(INPUT_RUN_1 + i, string::f("Run timer % d", currentNumber));

			configOutput(OUTPUT_TRIGGER_1 + i, string::f("Output on timer end %d", currentNumber));

			setTimerValues[i] = 1;
			currentTimerValues[i] = 1;
			timerStarted[i] = false;

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

					if (lastTimerEdge != internalTimerSecond) {
						if (timerStarted[i]) {
							decreaseTimer(i);
						}
						lastTimerEdge = internalTimerSecond;
					}

					lights[LIGHT_TIMER_1 + i].setBrightnessSmooth(1.f, args.sampleTime);
				}
			}
			else {
				if (!inputs[INPUT_TRIGGER_1 + i].isConnected()) {
					lights[LIGHT_TIMER_1 + i].setBrightnessSmooth(0.f, args.sampleTime);
				}
				lastTimerEdge = internalTimerSecond;
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

		SanguineLightUpSwitch* switchRestart1 = createParam<SanguineLightUpSwitch>(millimetersToPixelsVec(45.87, 14.631),
			module, Aion::PARAM_RESTART_1);
		switchRestart1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_off.svg")));
		switchRestart1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_on.svg")));
		switchRestart1->addHalo(nvgRGB(0, 0, 0));
		switchRestart1->addHalo(nvgRGB(0, 255, 0));
		switchRestart1->momentary = false;
		switchRestart1->latch = true;
		addParam(switchRestart1);

		SeqControlSwitch* btnTrigger1 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(2.835, 36.482),
			module, Aion::PARAM_TRIGGER_1);
		btnTrigger1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_off.svg")));
		btnTrigger1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_on.svg")));
		addParam(btnTrigger1);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 51.545), module, Aion::INPUT_TRIGGER_1));

		SeqControlSwitch* btnStart1 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(16.531, 36.482),
			module, Aion::PARAM_START_1);
		btnStart1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_off.svg")));
		btnStart1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_on.svg")));
		addParam(btnStart1);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 51.545), module, Aion::INPUT_RUN_1));

		SeqControlSwitch* btnReset1 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(30.246, 36.482),
			module, Aion::PARAM_RESET_1);
		btnReset1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset1);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.161, 51.545), module, Aion::INPUT_RESET_1));

		SanguineMonoOutputLight* lightOutput1 = new SanguineMonoOutputLight(module, 47.869, 44.546);		
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 51.545), module, Aion::OUTPUT_TRIGGER_1));

		// Timer 2

		SanguineLightUpSwitch* switchRestart2 = createParam<SanguineLightUpSwitch>(millimetersToPixelsVec(61.69, 14.631),
			module, Aion::PARAM_RESTART_2);
		switchRestart2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_off.svg")));
		switchRestart2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_on.svg")));
		switchRestart2->addHalo(nvgRGB(0, 0, 0));
		switchRestart2->addHalo(nvgRGB(0, 255, 0));
		switchRestart2->momentary = false;
		switchRestart2->latch = true;
		addParam(switchRestart2);

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(73.316, 27.047), module, Aion::PARAM_TIMER_2));

		SanguineTinyNumericDisplay* displayTimer2 = new SanguineTinyNumericDisplay(2, module, 90.004, 27.047);
		aionFramebuffer->addChild(displayTimer2);
		displayTimer2->fallbackNumber = 1;

		if (module)
			displayTimer2->values.numberValue = &module->currentTimerValues[1];

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(102.466, 27.047), module, Aion::LIGHT_TIMER_2));

		SeqControlSwitch* btnReset2 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(59.774, 36.482),
			module, Aion::PARAM_RESET_2);
		btnReset2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset2);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(63.69, 51.545), module, Aion::INPUT_RESET_2));

		SeqControlSwitch* btnStart2 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(73.565, 36.482),
			module, Aion::PARAM_START_2);
		btnStart2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_off.svg")));
		btnStart2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_on.svg")));
		addParam(btnStart2);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(77.48, 51.545), module, Aion::INPUT_RUN_2));

		SeqControlSwitch* btnTrigger2 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(87.326, 36.482),
			module, Aion::PARAM_TRIGGER_2);
		btnTrigger2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_off.svg")));
		btnTrigger2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_on.svg")));
		addParam(btnTrigger2);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(91.424, 51.545), module, Aion::INPUT_TRIGGER_2));

		SanguineMonoOutputLight* lightOutput2 = new SanguineMonoOutputLight(module, 105.016, 44.546);		
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(105.016, 51.545), module, Aion::OUTPUT_TRIGGER_2));

		// Timer 3

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(9.3, 73.246), module, Aion::LIGHT_TIMER_3));

		SanguineTinyNumericDisplay* displayTimer3 = new SanguineTinyNumericDisplay(2, module, 21.675, 73.246);
		aionFramebuffer->addChild(displayTimer3);
		displayTimer3->fallbackNumber = 1;

		if (module)
			displayTimer3->values.numberValue = &module->currentTimerValues[2];

		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.411, 73.246), module, Aion::PARAM_TIMER_3));

		SanguineLightUpSwitch* switchRestart3 = createParam<SanguineLightUpSwitch>(millimetersToPixelsVec(45.87, 60.833),
			module, Aion::PARAM_RESTART_3);
		switchRestart3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_off.svg")));
		switchRestart3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_on.svg")));
		switchRestart3->addHalo(nvgRGB(0, 0, 0));
		switchRestart3->addHalo(nvgRGB(0, 255, 0));
		switchRestart3->momentary = false;
		switchRestart3->latch = true;
		addParam(switchRestart3);

		SeqControlSwitch* btnTrigger3 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(2.835, 82.685),
			module, Aion::PARAM_TRIGGER_3);
		btnTrigger3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_off.svg")));
		btnTrigger3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_on.svg")));
		addParam(btnTrigger3);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.75, 97.748), module, Aion::INPUT_TRIGGER_3));

		SeqControlSwitch* btnStart3 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(16.531, 82.685),
			module, Aion::PARAM_START_3);
		btnStart3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_off.svg")));
		btnStart3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_on.svg")));
		addParam(btnStart3);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(20.446, 97.748), module, Aion::INPUT_RUN_3));

		SeqControlSwitch* btnReset3 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(30.346, 82.685),
			module, Aion::PARAM_RESET_3);
		btnReset3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset3);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.261, 97.748), module, Aion::INPUT_RESET_3));

		SanguineMonoOutputLight* lightOutput3 = new SanguineMonoOutputLight(module, 47.869, 90.749);
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(47.87, 97.748), module, Aion::OUTPUT_TRIGGER_3));

		// Timer 4

		SanguineLightUpSwitch* switchRestart4 = createParam<SanguineLightUpSwitch>(millimetersToPixelsVec(61.69, 60.833),
			module, Aion::PARAM_RESTART_4);
		switchRestart4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_off.svg")));
		switchRestart4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/restart_on.svg")));
		switchRestart4->addHalo(nvgRGB(0, 0, 0));
		switchRestart4->addHalo(nvgRGB(0, 255, 0));
		switchRestart4->momentary = false;
		switchRestart4->latch = true;
		addParam(switchRestart4);

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(73.316, 73.246), module, Aion::PARAM_TIMER_4));

		SanguineTinyNumericDisplay* displayTimer4 = new SanguineTinyNumericDisplay(2, module, 90.004, 73.246);
		aionFramebuffer->addChild(displayTimer4);
		displayTimer4->fallbackNumber = 1;

		if (module)
			displayTimer4->values.numberValue = &module->currentTimerValues[3];

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(102.466, 73.246), module, Aion::LIGHT_TIMER_4));

		SeqControlSwitch* btnReset4 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(59.774, 82.685),
			module, Aion::PARAM_RESET_4);
		btnReset4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset4);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(63.69, 97.748), module, Aion::INPUT_RESET_4));

		SeqControlSwitch* btnStart4 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(73.565, 82.685),
			module, Aion::PARAM_START_4);
		btnStart4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_off.svg")));
		btnStart4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_on.svg")));
		addParam(btnStart4);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(77.48, 97.748), module, Aion::INPUT_RUN_4));

		SeqControlSwitch* btnTrigger4 = createParamCentered<SeqControlSwitch>(millimetersToPixelsVec(87.326, 82.685),
			module, Aion::PARAM_TRIGGER_4);
		btnTrigger4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_off.svg")));
		btnTrigger4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/clock_on.svg")));
		addParam(btnTrigger4);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(91.242, 97.748), module, Aion::INPUT_TRIGGER_4));

		SanguineMonoOutputLight* lightOutput4 = new SanguineMonoOutputLight(module, 105.016, 90.749);
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(105.046, 97.748), module, Aion::OUTPUT_TRIGGER_4));

		// Sanguine logo lights

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 46.116, 113.48);		
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 59.248, 120.435);
		addChild(monstersLight);
	}
};

Model* modelAion = createModel<Aion, AionWidget>("Sanguine-Aion");