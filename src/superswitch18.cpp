#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#ifndef METAMODULE
#include "seqcomponents.hpp"
#endif
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "pcg_random.hpp"
#pragma GCC diagnostic pop

#include "switches.hpp"
#ifndef METAMODULE
#include "manus.hpp"
#endif

using simd::float_4;

struct SuperSwitch18 : SanguineModule {

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
		INPUT_IN,
		INPUT_STEPS,
		INPUT_DECREASE,
		INPUT_INCREASE,
		INPUT_RANDOM,
		INPUT_RESET,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT1,
		OUTPUT_OUT2,
		OUTPUT_OUT3,
		OUTPUT_OUT4,
		OUTPUT_OUT5,
		OUTPUT_OUT6,
		OUTPUT_OUT7,
		OUTPUT_OUT8,
		OUTPUTS_COUNT
	};

	enum LightIds {
#ifndef METAMODULE
		LIGHT_EXPANDER,
#endif
#ifdef METAMODULE
		LIGHT_NO_REPEATS,
		LIGHT_RESET_TO_FIRST_STEP,
		LIGHT_ONE_SHOT,
		ENUMS(LIGHT_STEP_1, 2),
		ENUMS(LIGHT_STEP_2, 2),
		ENUMS(LIGHT_STEP_3, 2),
		ENUMS(LIGHT_STEP_4, 2),
		ENUMS(LIGHT_STEP_5, 2),
		ENUMS(LIGHT_STEP_6, 2),
		ENUMS(LIGHT_STEP_7, 2),
		ENUMS(LIGHT_STEP_8, 2),
#endif
		LIGHTS_COUNT
	};

	dsp::BooleanTrigger btDecrease;
	dsp::BooleanTrigger btIncrease;
	dsp::BooleanTrigger btRandom;
	dsp::BooleanTrigger btReset;
	dsp::BooleanTrigger btSteps[superSwitches::kMaxSteps];
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
#ifndef METAMODULE
	dsp::SchmittTrigger stDirectSteps[superSwitches::kMaxSteps];
	Manus* manusExpander = nullptr;
#endif

	bool bLastOneShotValue = false;
	bool bLastResetToFirstStepValue = true;
	bool bNoRepeats = false;
	bool bOneShot = false;
	bool bOneShotDone = false;
	bool bResetToFirstStep = true;
	bool bResetMutex = false;
	bool bStepsMutex = false;

	bool bHaveStepsCable = false;
	bool bHaveResetCable = false;
	bool bHaveDecreaseCable = false;
	bool bHaveIncreaseCable = false;
	bool bHaveRandomCable = false;
	bool bInputConnected = false;
	bool outputsConnected[superSwitches::kMaxSteps] = {};

#ifndef METAMODULE
	bool bHasExpander = false;
#endif

	int channelCount = 0;
	int randomNum = 0;
	int selectedOut = 0;
	int stepCount = superSwitches::kMaxSteps;
	int stepsDone = 0;

	static const int kLightsFrequency = 16;

	float_4 outVoltages[4] = {};

	dsp::ClockDivider lightsDivider;

	pcg32 pcgRng;

	SuperSwitch18() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_STEPS, 1.f, 8.f, 8.f, "Steps", "", 0.f, 1.f, 0.f);
		paramQuantities[PARAM_STEPS]->snapEnabled = true;

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");
		configButton(PARAM_RESET_TO_FIRST_STEP, "Reset to first step");
		configButton(PARAM_ONE_SHOT, "One shot");

		int componentNum;
		for (int component = 0; component < superSwitches::kMaxSteps; ++component) {
			componentNum = component + 1;
			configButton(PARAM_STEP1 + component, string::f("Step %d", componentNum));
			configOutput(OUTPUT_OUT1 + component, string::f("Voltage %d", componentNum));
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
		configInput(INPUT_IN, "Voltage");
		params[PARAM_STEP1].setValue(1);
		params[PARAM_RESET_TO_FIRST_STEP].setValue(1);
		pcgRng = pcg32(static_cast<int>(std::round(system::getUnixTime())));
		lightsDivider.setDivision(kLightsFrequency);
	};

#ifndef METAMODULE
	void process(const ProcessArgs& args) override {
		channelCount = inputs[INPUT_IN].getChannels();

		bool bIsLightsTurn = lightsDivider.process();

		float sampleTime;

		if (bIsLightsTurn) {
			sampleTime = args.sampleTime * kLightsFrequency;

			handleParameterControls();

			lights[LIGHT_EXPANDER].setBrightnessSmooth(
				bHasExpander * kSanguineButtonLightValue, sampleTime);
		}

		checkReset();

		handleClockControls();

		if (!bHasExpander) {
			for (int stepNum = 0; stepNum < superSwitches::kMaxSteps; ++stepNum) {
				handleStepButtons(stepNum);

				params[stepNum].setValue(stepNum == selectedOut ? 1 : stepNum < stepCount ? 0 : 2);
			}

			copyVoltages();

			setUnselectedOutputs();

			bResetMutex = false;
		} else {
			for (int stepNum = 0; stepNum < superSwitches::kMaxSteps; ++stepNum) {
				handleStepButtons(stepNum);

				handleExpanderInput(stepNum);

				params[stepNum].setValue(stepNum == selectedOut ? 1 : stepNum < stepCount ? 0 : 2);
			}

			copyVoltages();

			setUnselectedOutputs();

			if (bIsLightsTurn) {
				setExpanderLights(sampleTime);
			}
			bResetMutex = false;
		}
	}
#else
	void process(const ProcessArgs& args) override {
		channelCount = inputs[INPUT_IN].getChannels();

		bool bIsLightsTurn = lightsDivider.process();

		float sampleTime;

		if (bIsLightsTurn) {
			sampleTime = args.sampleTime * kLightsFrequency;

			handleParameterControls();
		}

		checkReset();

		handleClockControls();

		for (int stepNum = 0; stepNum < superSwitches::kMaxSteps; ++stepNum) {
			handleStepButtons(stepNum);

			params[stepNum].setValue(stepNum == selectedOut ? 1 : stepNum < stepCount ? 0 : 2);
		}

		copyVoltages();

		setUnselectedOutputs();

		if (bIsLightsTurn) {
			int currentLight
				for (int step = 0; step < superSwitches::kMaxSteps; ++step) {
					currentLight = LIGHT_STEP_1 + step * 2;
					if (step < stepCount) {
						lights[currentLight].setBrightness((step != selectedOut) *
							kSanguineButtonLightValue);
						lights[currentLight + 1].setBrightness((step == selectedOut) *
							kSanguineButtonLightValue);
					} else {
						lights[currentLight].setBrightness(0.f);
						lights[currentLight + 1].setBrightness(0.f);
					}
				}

			lights[LIGHT_RESET_TO_FIRST_STEP].setBrightness(
				params[PARAM_RESET_TO_FIRST_STEP].getValue() * kSanguineButtonLightValue);
			lights[LIGHT_ONE_SHOT].setBrightness(params[PARAM_ONE_SHOT].getValue() *
				kSanguineButtonLightValue);
			lights[LIGHT_NO_REPEATS].setBrightness(params[PARAM_NO_REPEATS].getValue() *
				kSanguineButtonLightValue);
		}
		bResetMutex = false;
	}
#endif

	void handleParameterControls() {
		bStepsMutex = false;
		int knobValue = params[PARAM_STEPS].getValue();
		if (bHaveStepsCable) {
			int newStepCount = round(clamp(inputs[INPUT_STEPS].getVoltage(), 1.f, 8.f));
			if (newStepCount != stepCount) {
				stepCount = newStepCount;
				bStepsMutex = true;
			}
		} else if (knobValue != stepCount) {
			stepCount = knobValue;
			bStepsMutex = true;
		}

		if (selectedOut >= stepCount) {
			params[selectedOut].setValue(0);
			selectedOut = stepCount - 1;
			params[selectedOut].setValue(1);
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
		bResetToFirstStep = params[PARAM_RESET_TO_FIRST_STEP].getValue();
		if (!bLastResetToFirstStepValue && bResetToFirstStep) {
			selectedOut = 0;
		}
		bLastResetToFirstStepValue = bResetToFirstStep;
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && (bOneShot != bLastOneShotValue)) {
			bOneShotDone = false;
			stepsDone = 0;
		}
		bLastOneShotValue = bOneShot;
	}

	void checkReset() {
		if ((bHaveResetCable && stInputReset.process(inputs[INPUT_RESET].getVoltage())) ||
			btReset.process(params[PARAM_RESET].getValue())) {
			doResetTrigger();
		}
	}

	void handleClockControls() {
		if (!bOneShot || !bOneShotDone) {
			if ((bHaveDecreaseCable && stInputDecrease.process(inputs[INPUT_DECREASE].getVoltage())) ||
				btDecrease.process(params[PARAM_DECREASE].getValue())) {
				doDecreaseTrigger();
			}

			if ((bHaveIncreaseCable && stInputIncrease.process(inputs[INPUT_INCREASE].getVoltage())) ||
				btIncrease.process(params[PARAM_INCREASE].getValue())) {
				doIncreaseTrigger();
			}

			if ((bHaveRandomCable && stInputRandom.process(inputs[INPUT_RANDOM].getVoltage())) ||
				btRandom.process(params[PARAM_RANDOM].getValue())) {
				doRandomTrigger();
			}
		}
	}

	void handleStepButtons(const int stepNum) {
		const int currentOut = PARAM_STEP1 + stepNum;
		if (stepNum < stepCount && !bResetMutex && (!bOneShot || !bOneShotDone) &&
			btSteps[stepNum].process(params[currentOut].getValue() && !bStepsMutex)) {
			selectedOut = stepNum;
		}
	}

	void copyVoltages() {
		if (selectedOut >= 0 && bInputConnected && outputsConnected[selectedOut]) {
			int currentChannel;
			for (int channel = 0; channel < channelCount; channel += 4) {
				currentChannel = channel >> 2;
				outVoltages[currentChannel] = inputs[INPUT_IN].getVoltageSimd<float_4>(channel);
				outputs[selectedOut].setVoltageSimd(outVoltages[currentChannel], channel);
			}
			outputs[selectedOut].setChannels(channelCount);
		}
	}

	void setUnselectedOutputs() {
		int currentOut;
		for (int outNum = 0; outNum < superSwitches::kMaxSteps; ++outNum) {
			currentOut = OUTPUT_OUT1 + outNum;
			if (outputsConnected[outNum] && outNum != selectedOut) {
				outputs[currentOut].setChannels(0);
			}
		}
	}

	void doDecreaseTrigger() {
		--selectedOut;

		if (selectedOut < 0) {
			selectedOut = stepCount - 1;
		}

		++stepsDone;
		if (bOneShot && stepsDone == stepCount) {
			bOneShotDone = true;
		}
		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	}

	void doIncreaseTrigger() {
		++selectedOut;

		if (selectedOut >= stepCount) {
			selectedOut = 0;
		}

		++stepsDone;
		if (bOneShot && stepsDone == stepCount) {
			bOneShotDone = true;
		}

		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	};

	void doRandomTrigger() {
		if (!bNoRepeats) {
			selectedOut = pcgRng(stepCount);
		} else {
			randomNum = selectedOut;
			while (randomNum == selectedOut)
				randomNum = pcgRng(stepCount);
			selectedOut = randomNum;
		}

		++stepsDone;
		if (bOneShot && stepsDone == stepCount) {
			bOneShotDone = true;
		}

		if (stepsDone > stepCount) {
			stepsDone = 0;
		}
	}

	void doResetTrigger() {
		selectedOut = bResetToFirstStep ? 0 : -1;
		stepsDone = 0;
		bOneShotDone = false;
		bLastOneShotValue = bOneShot;
		bLastResetToFirstStepValue = bResetToFirstStep;
		bResetMutex = true;
	};

	void onReset() override {
		if (bResetToFirstStep) {
			selectedOut = 0;
		} else {
			channelCount = 0;

			selectedOut = -1;
		}
		stepCount = superSwitches::kMaxSteps;

		params[PARAM_STEPS].setValue(superSwitches::kMaxSteps);
		params[PARAM_RESET_TO_FIRST_STEP].setValue(bResetToFirstStep);
		bStepsMutex = false;
		bResetMutex = false;

		for (int out = 0; out < superSwitches::kMaxSteps; ++out) {
			params[PARAM_STEP1 + out].setValue(0);
		}

		if (selectedOut == 0) {
			params[PARAM_STEP1].setValue(1);
		}
	}

	void onRandomize() override {
		stepCount = pcgRng(superSwitches::kMaxSteps) + 1;
		selectedOut = pcgRng(stepCount);
		params[PARAM_STEPS].setValue(stepCount);
	}

#ifndef METAMODULE
	void handleExpanderInput(const int stepNum) {
		int currentIn = Manus::INPUT_STEP_1 + stepNum;
		if (manusExpander->getInputConnected(stepNum) && stepNum < stepCount &&
			(!bOneShot || !bOneShotDone) &&
			stDirectSteps[stepNum].process(manusExpander->getInput(currentIn).getVoltage())) {
			selectedOut = stepNum;
		}
	}

	void setExpanderLights(const float sampleTime) {
		bool bLightActive;
		int currentLight;
		for (int step = 0; step < superSwitches::kMaxSteps; ++step) {
			currentLight = Manus::LIGHT_STEP_1_LEFT + step;
			bLightActive = step < stepCount;
			manusExpander->getLight(currentLight).setBrightnessSmooth(
				bLightActive * kSanguineButtonLightValue, sampleTime);
		}
	}

	void onBypass(const BypassEvent& e) override {
		if (bHasExpander) {
			manusExpander->getLight(Manus::LIGHT_MASTER_MODULE_LEFT).setBrightness(0.f);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHasExpander) {
			manusExpander->getLight(Manus::LIGHT_MASTER_MODULE_LEFT).setBrightness(kSanguineButtonLightValue);
		}
		Module::onUnBypass(e);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		if (e.side == 1) {
			Module* rightModule = getRightExpander().module;

			bHasExpander = (rightModule && rightModule->getModel() == modelManus && !rightModule->isBypassed());

			if (bHasExpander) {
				manusExpander = dynamic_cast<Manus*>(rightModule);
			}
		}
	}
#endif

	void onPortChange(const PortChangeEvent& e) override {
		switch (e.type) {
		case Port::INPUT:
			switch (e.portId) {
			case INPUT_STEPS:
				bHaveStepsCable = e.connecting;
				break;

			case INPUT_RESET:
				bHaveResetCable = e.connecting;
				break;

			case INPUT_DECREASE:
				bHaveDecreaseCable = e.connecting;
				break;

			case INPUT_INCREASE:
				bHaveIncreaseCable = e.connecting;
				break;

			case INPUT_RANDOM:
				bHaveRandomCable = e.connecting;
				break;

			case INPUT_IN:
				bInputConnected = e.connecting;
				break;
			}
			break;

		case Port::OUTPUT:
			outputsConnected[e.portId] = e.connecting;
			break;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonBoolean(rootJ, "noRepeats", bNoRepeats);
		setJsonBoolean(rootJ, "ResetToFirstStep", bResetToFirstStep);
		setJsonBoolean(rootJ, "oneShot", bOneShot);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		getJsonBoolean(rootJ, "noRepeats", bNoRepeats);

		getJsonBoolean(rootJ, "ResetToFirstStep", bResetToFirstStep);

		bLastResetToFirstStepValue = bResetToFirstStep;
		if (!bResetToFirstStep) {
			selectedOut = -1;
		} else {
			selectedOut = 0;
		}

		getJsonBoolean(rootJ, "oneShot", bOneShot);
		params[PARAM_ONE_SHOT].setValue(bOneShot);

		if (bOneShot && bOneShot != bLastOneShotValue) {
			bOneShotDone = false;
		}
		bLastOneShotValue = bOneShot;
	}
};

struct SuperSwitch18Widget : SanguineModuleWidget {
	explicit SuperSwitch18Widget(SuperSwitch18* module) {
		setModule(module);

		moduleName = "switch1-8";
		panelSize = SIZE_13;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(63.44f, 5.573), module, SuperSwitch18::LIGHT_EXPANDER));
#endif

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(9.844, 18.272), module, SuperSwitch18::PARAM_STEPS));
		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(9.844, 32.461), module, SuperSwitch18::INPUT_STEPS));

		float currentY = 23.904f;
		float deltaY = 13.129f;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT1));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT2));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT3));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT4));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT5));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT6));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT7));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(59.887, currentY), module, SuperSwitch18::OUTPUT_OUT8));

#ifndef METAMODULE
		currentY = 20.104f;

		addParam(createParam<SeqStep1Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP1));
		currentY += deltaY;

		addParam(createParam<SeqStep2Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP2));
		currentY += deltaY;

		addParam(createParam<SeqStep3Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP3));
		currentY += deltaY;

		addParam(createParam<SeqStep4Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP4));
		currentY += deltaY;

		addParam(createParam<SeqStep5Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP5));
		currentY += deltaY;

		addParam(createParam<SeqStep6Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP6));
		currentY += deltaY;

		addParam(createParam<SeqStep7Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP7));
		currentY += deltaY;

		addParam(createParam<SeqStep8Big>(millimetersToPixelsVec(39.677, currentY), module, SuperSwitch18::PARAM_STEP8));

		addParam(createParam<SeqButtonNoRepeatsSmall>(millimetersToPixelsVec(28.711, 43.085f), module, SuperSwitch18::PARAM_NO_REPEATS));
		addParam(createParam<SeqButtonOneShotSmall>(millimetersToPixelsVec(15.844, 43.085f), module, SuperSwitch18::PARAM_ONE_SHOT));
		addParam(createParam<SeqButtonResetToOne>(millimetersToPixelsVec(2.977, 43.085f), module, SuperSwitch18::PARAM_RESET_TO_FIRST_STEP));

		addParam(createParamCentered<SeqButtonUp>(millimetersToPixelsVec(25.349, 55.292), module, SuperSwitch18::PARAM_INCREASE));
		addParam(createParamCentered<SeqButtonDown>(millimetersToPixelsVec(25.349, 69.922), module, SuperSwitch18::PARAM_DECREASE));
		addParam(createParamCentered<SeqButtonRandom>(millimetersToPixelsVec(25.349, 84.553), module, SuperSwitch18::PARAM_RANDOM));
		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(25.349, 99.184), module, SuperSwitch18::PARAM_RESET));
#else
		currentY = 23.904f;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP1, SuperSwitch18::LIGHT_STEP_1));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP2, SuperSwitch18::LIGHT_STEP_2));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP3, SuperSwitch18::LIGHT_STEP_3));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP4, SuperSwitch18::LIGHT_STEP_4));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP5, SuperSwitch18::LIGHT_STEP_5));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP6, SuperSwitch18::LIGHT_STEP_6));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP7, SuperSwitch18::LIGHT_STEP_7));
		currentY += deltaY;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(44.841, currentY),
			module, SuperSwitch18::PARAM_STEP8, SuperSwitch18::LIGHT_STEP_8));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(millimetersToPixelsVec(4.977, 43.985), module,
			SuperSwitch18::PARAM_RESET_TO_FIRST_STEP, SuperSwitch18::LIGHT_RESET_TO_FIRST_STEP));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(17.844, 43.985), module,
			SuperSwitch18::PARAM_ONE_SHOT, SuperSwitch18::LIGHT_ONE_SHOT));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<PurpleLight>>>(millimetersToPixelsVec(30.711, 43.985), module,
			SuperSwitch18::PARAM_NO_REPEATS, SuperSwitch18::LIGHT_NO_REPEATS));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.233, 55.291), module, SuperSwitch18::PARAM_INCREASE));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.233, 69.922), module, SuperSwitch18::PARAM_DECREASE));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.233, 84.553), module, SuperSwitch18::PARAM_RANDOM));
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.233, 99.184), module, SuperSwitch18::PARAM_RESET));
#endif

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_INCREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_DECREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_RESET));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(10.237, 116.012), module, SuperSwitch18::INPUT_IN));

		FramebufferWidget* switchFrameBuffer = new FramebufferWidget();
		addChild(switchFrameBuffer);

		SanguineLedNumberDisplay* display = new SanguineLedNumberDisplay(2, module, 26.644, 21.472);
		switchFrameBuffer->addChild(display);
		display->fallbackNumber = superSwitches::kMaxSteps;

		if (module) {
			display->values.numberValue = (&module->stepCount);
		}

#ifndef METAMODULE
		SanguineStaticRGBLight* stepsLight = new SanguineStaticRGBLight(module, "res/seqs/light_steps.svg", 27.015, 34.372,
			true, kSanguineBlueLight);
		addChild(stepsLight);

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 10.237, 108.611);
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 53.4, 15.114);
		addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight(module, "res/blood_glowy_small.svg", 26.154, 112.707);
		addChild(bloodLogo);
#endif
	}

#ifndef METAMODULE
	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		SuperSwitch18* superSwitch18 = dynamic_cast<SuperSwitch18*>(this->module);

		menu->addChild(new MenuSeparator());
		const Module* expander = superSwitch18->rightExpander.module;
		if (expander && expander->model == modelManus) {
			menu->addChild(createMenuLabel("Manus expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Manus expander", "", [=]() {
				superSwitch18->addExpander(modelManus, this);
				}));
		}
	}
#endif
};

Model* modelSuperSwitch18 = createModel<SuperSwitch18, SuperSwitch18Widget>("Sanguine-SuperSwitch18");