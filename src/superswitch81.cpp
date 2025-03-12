#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"
#include "pcg_random.hpp"
#include "switches.hpp"
#include "manus.hpp"

using simd::float_4;

struct SuperSwitch81 : SanguineModule {

	enum ParamIds {
		PARAM_STEP1,
		PARAM_STEP2,
		PARAM_STEP3,
		PARAM_STEP4,
		PARAM_STEP5,
		PARAM_STEP6,
		PARAM_STEP7,
		PARAM_STEP8,
		PARAM_STEPS,
		PARAM_NO_REPEATS,
		PARAM_DECREASE,
		PARAM_INCREASE,
		PARAM_RANDOM,
		PARAM_RESET,
		PARAM_RESET_TO_FIRST_STEP,
		PARAM_ONE_SHOT,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_IN1,
		INPUT_IN2,
		INPUT_IN3,
		INPUT_IN4,
		INPUT_IN5,
		INPUT_IN6,
		INPUT_IN7,
		INPUT_IN8,
		INPUT_STEPS,
		INPUT_DECREASE,
		INPUT_INCREASE,
		INPUT_RANDOM,
		INPUT_RESET,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_EXPANDER,
		LIGHTS_COUNT
	};

	dsp::BooleanTrigger btDecrease;
	dsp::BooleanTrigger btIncrease;
	dsp::BooleanTrigger btRandom;
	dsp::BooleanTrigger btReset;
	dsp::BooleanTrigger btSteps[kMaxSteps];
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
	dsp::SchmittTrigger stDirectSteps[kMaxSteps];
	bool bClockReceived = false;
	bool bLastOneShotValue = false;
	bool bLastResetToFirstStepValue = true;
	bool bNoRepeats = false;
	bool bOneShot = false;
	bool bOneShotDone = false;
	bool bResetToFirstStep = true;
	bool bHasExpander = false;
	int inChannelCount = 0;
	int randomNum = 0;
	int selectedIn = 0;
	int stepCount = kMaxSteps;
	int stepsDone = 0;

	static const int kClockDivider = 16;

	float_4 outVoltages[4] = {};

	dsp::ClockDivider clockDivider;

	pcg32 pcgRng;

	SuperSwitch81() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_STEPS, 1.f, 8.f, 8.f, "Steps", "", 0.f, 1.f, 0.f);
		paramQuantities[PARAM_STEPS]->snapEnabled = true;

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");
		configButton(PARAM_RESET_TO_FIRST_STEP, "Reset to first step");
		configButton(PARAM_ONE_SHOT, "One shot");

		for (int component = 0; component < kMaxSteps; ++component) {
			configButton(PARAM_STEP1 + component, "Step " + std::to_string(component + 1));
			configInput(INPUT_IN1 + component, "Voltage " + std::to_string(component + 1));
		}
		configInput(INPUT_STEPS, "Step count");

		configButton(PARAM_DECREASE, "Previous step");
		configButton(PARAM_INCREASE, "Next step");
		configButton(PARAM_RANDOM, "Random step");
		configButton(PARAM_RESET, "Reset");

		configInput(INPUT_DECREASE, "Trigger previous step");
		configInput(INPUT_INCREASE, "Trigger next step");
		configInput(INPUT_RANDOM, "Trigger random");
		configInput(INPUT_RESET, "Trigger reset");
		configOutput(OUTPUT_OUT, "Voltage");
		params[PARAM_STEP1].setValue(1);
		params[PARAM_RESET_TO_FIRST_STEP].setValue(1);
		pcgRng = pcg32(static_cast<int>(std::round(system::getUnixTime())));

		clockDivider.setDivision(kClockDivider);
	};

	void doDecreaseTrigger() {
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
			--selectedIn;

			if (selectedIn < 0) {
				selectedIn = stepCount - 1;
			}
		} else {
			selectedIn = stepCount - 1;
			bClockReceived = true;
		}
		++stepsDone;
		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	};

	void doIncreaseTrigger() {
		++selectedIn;

		if (selectedIn >= stepCount) {
			selectedIn = 0;
		}

		bClockReceived = true;
		++stepsDone;

		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	};

	void doRandomTrigger() {
		if (!bNoRepeats) {
			selectedIn = pcgRng(stepCount);
		} else {
			randomNum = selectedIn;
			while (randomNum == selectedIn)
				randomNum = pcgRng(stepCount);
			selectedIn = randomNum;
		}
		bClockReceived = true;
		++stepsDone;
		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	};

	void doResetTrigger() {
		if (bResetToFirstStep) {
			selectedIn = 0;
		} else {
			selectedIn = -1;
			bClockReceived = false;
			outputs[OUTPUT_OUT].setChannels(0);
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel += 4) {
				outVoltages[channel / 4] = 0.f;
			}
		}
		stepsDone = 0;
		if (bOneShot)
			bOneShotDone = false;
	};

	void process(const ProcessArgs& args) override {
		Module* manusExpander = getLeftExpander().module;

		bHasExpander = (manusExpander && manusExpander->getModel() == modelManus && !manusExpander->isBypassed());

		if (clockDivider.process()) {
			const float sampleTime = args.sampleTime * kClockDivider;

			if (inputs[INPUT_STEPS].isConnected()) {
				stepCount = round(clamp(inputs[INPUT_STEPS].getVoltage(), 1.f, 8.f));
			} else if (params[PARAM_STEPS].getValue() != stepCount) {
				stepCount = params[PARAM_STEPS].getValue();
			}

			for (int step = 0; step < kMaxSteps; ++step) {
				if (step < stepCount) {
					params[PARAM_STEP1 + step].setValue(step == selectedIn ? 1 : 0);
				} else {
					params[PARAM_STEP1 + step].setValue(2);
				}
			}

			lights[LIGHT_EXPANDER].setBrightnessSmooth(bHasExpander ? kSanguineButtonLightValue : 0.f, sampleTime);

			if (bHasExpander) {
				for (int step = 0; step < kMaxSteps; ++step) {
					int currentLight = Manus::LIGHT_STEP_1_RIGHT + step;
					if (step < stepCount) {
						manusExpander->getLight(currentLight).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
					} else {
						manusExpander->getLight(currentLight).setBrightnessSmooth(0.f, sampleTime);
					}
				}
			}
		}

		for (int channel = 0; channel < PORT_MAX_CHANNELS; channel += 4) {
			outVoltages[channel / 4] = 0.f;
		}

		if ((inputs[INPUT_RESET].isConnected() && stInputReset.process(inputs[INPUT_RESET].getVoltage()))
			|| btReset.process(params[PARAM_RESET].getValue())) {
			doResetTrigger();
		}

		if (!bOneShot || (bOneShot && !bOneShotDone)) {
			if ((inputs[INPUT_DECREASE].isConnected() && stInputDecrease.process(inputs[INPUT_DECREASE].getVoltage()))
				|| btDecrease.process(params[PARAM_DECREASE].getValue())) {
				doDecreaseTrigger();
			}

			if ((inputs[INPUT_INCREASE].isConnected() && stInputIncrease.process(inputs[INPUT_INCREASE].getVoltage()))
				|| btIncrease.process(params[PARAM_INCREASE].getValue())) {
				doIncreaseTrigger();
			}

			if ((inputs[INPUT_RANDOM].isConnected() && stInputRandom.process(inputs[INPUT_RANDOM].getVoltage()))
				|| btRandom.process(params[PARAM_RANDOM].getValue())) {
				doRandomTrigger();
			}

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				for (int step = 0; step < stepCount; ++step) {
					if (bHasExpander) {
						const int currentInput = Manus::INPUT_STEP_1 + step;
						if (stDirectSteps[step].process(manusExpander->getInput(currentInput).getVoltage()) && step < stepCount) {
							selectedIn = step;
						}
					}

					if (btSteps[step].process(params[PARAM_STEP1 + step].getValue()) && step < stepCount) {
						selectedIn = step;
					}
				}

				if (inputs[INPUT_IN1 + selectedIn].isConnected()) {
					inChannelCount = inputs[INPUT_IN1 + selectedIn].getChannels();
					for (int channel = 0; channel < inChannelCount; channel += 4) {
						outVoltages[channel / 4] = inputs[INPUT_IN1 + selectedIn].getVoltageSimd<float_4>(channel);
					}
				}
			}

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				if (outputs[OUTPUT_OUT].isConnected()) {
					outputs[OUTPUT_OUT].setChannels(inChannelCount);
					for (int channel = 0; channel < inChannelCount; channel += 4) {
						outputs[OUTPUT_OUT].setVoltageSimd(outVoltages[channel / 4], channel);
					}
				}

				if (bOneShot && stepsDone == stepCount) {
					bOneShotDone = true;
				}
			}
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
		bResetToFirstStep = params[PARAM_RESET_TO_FIRST_STEP].getValue();
		if (!bLastResetToFirstStepValue && bResetToFirstStep) {
			selectedIn = 0;
		}
		bLastResetToFirstStepValue = bResetToFirstStep;

		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && (bOneShot != bLastOneShotValue)) {
			bOneShotDone = false;
		}
		bLastOneShotValue = bOneShot;
	};

	void onReset() override {
		if (bResetToFirstStep) {
			selectedIn = 0;
		} else {
			selectedIn = -1;
			bClockReceived = false;
		}
		stepCount = kMaxSteps;
	}

	void onRandomize() override {
		stepCount = pcgRng(kMaxSteps) + 1;
		selectedIn = pcgRng(stepCount);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "noRepeats", json_boolean(bNoRepeats));
		json_object_set_new(rootJ, "resetToFirstStep", json_boolean(bResetToFirstStep));
		return rootJ;
	}

	void onBypass(const BypassEvent& e) override {
		if (bHasExpander) {
			Module* manusExpander = getRightExpander().module;
			manusExpander->getLight(Manus::LIGHT_MASTER_MODULE_LEFT).setBrightness(0.f);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHasExpander) {
			Module* manusExpander = getRightExpander().module;
			manusExpander->getLight(Manus::LIGHT_MASTER_MODULE_LEFT).setBrightness(kSanguineButtonLightValue);
		}
		Module::onUnBypass(e);
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* noRepeatsJ = json_object_get(rootJ, "noRepeats");
		if (noRepeatsJ) {
			bNoRepeats = json_boolean_value(noRepeatsJ);
		}

		json_t* resetToFirstStepJ = json_object_get(rootJ, "resetToFirstStep");
		bResetToFirstStep = json_boolean_value(resetToFirstStepJ);
		bLastResetToFirstStepValue = bResetToFirstStep;
		if (!bResetToFirstStep) {
			selectedIn = -1;
			bClockReceived = false;
		} else {
			selectedIn = 0;
		}
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && bOneShot != bLastOneShotValue) {
			bOneShotDone = false;
		}
		bLastOneShotValue = bOneShot;
	}
};

struct SuperSwitch81Widget : SanguineModuleWidget {
	explicit SuperSwitch81Widget(SuperSwitch81* module) {
		setModule(module);

		moduleName = "switch8-1";
		panelSize = SIZE_13;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.6f, 5.573f), module, SuperSwitch81::LIGHT_EXPANDER));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(56.197, 18.272), module, SuperSwitch81::PARAM_STEPS));
		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(56.197, 32.461), module, SuperSwitch81::INPUT_STEPS));

		float currentY = 23.904f;
		float deltaY = 13.129f;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN1));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN2));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN3));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN4));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN5));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN6));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN7));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.153, currentY), module, SuperSwitch81::INPUT_IN8));

		currentY = 20.104f;

		addParam(createParam<SeqStep1Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP1));
		currentY += deltaY;

		addParam(createParam<SeqStep2Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP2));
		currentY += deltaY;

		addParam(createParam<SeqStep3Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP3));
		currentY += deltaY;

		addParam(createParam<SeqStep4Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP4));
		currentY += deltaY;

		addParam(createParam<SeqStep5Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP5));
		currentY += deltaY;

		addParam(createParam<SeqStep6Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP6));
		currentY += deltaY;

		addParam(createParam<SeqStep7Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP7));
		currentY += deltaY;

		addParam(createParam<SeqStep8Big>(millimetersToPixelsVec(18.763, currentY), module, SuperSwitch81::PARAM_STEP8));

		currentY = 43.085f;

		addParam(createParam<SeqButtonNoRepeatsSmall>(millimetersToPixelsVec(33.4, currentY), module, SuperSwitch81::PARAM_NO_REPEATS));

		addParam(createParam<SeqButtonOneShotSmall>(millimetersToPixelsVec(46.3, currentY), module, SuperSwitch81::PARAM_ONE_SHOT));

		addParam(createParam<SeqButtonResetToOne>(millimetersToPixelsVec(59.2, currentY), module, SuperSwitch81::PARAM_RESET_TO_FIRST_STEP));

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.553, currentY), module, SuperSwitch81::INPUT_INCREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.553, currentY), module, SuperSwitch81::INPUT_DECREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.553, currentY), module, SuperSwitch81::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.553, currentY), module, SuperSwitch81::INPUT_RESET));

		addParam(createParamCentered<SeqButtonUp>(millimetersToPixelsVec(40.692, 55.292), module, SuperSwitch81::PARAM_INCREASE));

		addParam(createParamCentered<SeqButtonDown>(millimetersToPixelsVec(40.692, 69.922), module, SuperSwitch81::PARAM_DECREASE));

		addParam(createParamCentered<SeqButtonRandom>(millimetersToPixelsVec(40.692, 84.553), module, SuperSwitch81::PARAM_RANDOM));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(40.692, 99.184), module, SuperSwitch81::PARAM_RESET));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(55.803, 116.012), module, SuperSwitch81::OUTPUT_OUT));

		FramebufferWidget* switchFrameBuffer = new FramebufferWidget();
		addChild(switchFrameBuffer);

		SanguineLedNumberDisplay* display = new SanguineLedNumberDisplay(2, module, 39.397, 21.472);
		switchFrameBuffer->addChild(display);
		display->fallbackNumber = kMaxSteps;

		if (module) {
			display->values.numberValue = (&module->stepCount);
		}

		SanguineStaticRGBLight* stepsLight = new SanguineStaticRGBLight(module, "res/seqs/light_steps.svg", 39.026, 34.372, true, kSanguineBlueLight);
		addChild(stepsLight);

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 13.041, 15.714);
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 55.803, 108.311);
		addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight(module, "res/blood_glowy_small.svg", 35.585, 112.707);
		addChild(bloodLogo);
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		SuperSwitch81* superSwitch81 = dynamic_cast<SuperSwitch81*>(this->module);

		menu->addChild(new MenuSeparator());
		const Module* expander = superSwitch81->leftExpander.module;
		if (expander && expander->model == modelManus) {
			menu->addChild(createMenuLabel("Manus expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Manus expander", "", [=]() {
				superSwitch81->addExpander(modelManus, this, SanguineModule::EXPANDER_LEFT);
				}));
		}
	}
};

Model* modelSuperSwitch81 = createModel<SuperSwitch81, SuperSwitch81Widget>("Sanguine-SuperSwitch81");