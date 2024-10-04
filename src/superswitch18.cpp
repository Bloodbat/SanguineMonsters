#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"
#include "pcg_variants.h"

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

	dsp::BooleanTrigger btDecrease;
	dsp::BooleanTrigger btIncrease;
	dsp::BooleanTrigger btRandom;
	dsp::BooleanTrigger btReset;
	dsp::BooleanTrigger btSteps[8];
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
	bool bClockReceived = false;
	bool bLastOneShotValue = false;
	bool bLastResetToFirstStepValue = true;
	bool bNoRepeats = false;
	bool bOneShot = false;
	bool bOneShotDone = false;
	bool bResetToFirstStep = true;
	int channelCount = 0;
	int randomNum;
	int selectedOut = 0;
	int stepCount = 8;
	int stepsDone = 0;

	float_4 outVoltages[4] = {};

	dsp::ClockDivider clockDivider;

	pcg32_random_t pcgRng;

	SuperSwitch18() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, 0);

		configParam(PARAM_STEPS, 1.0f, 8.0f, 8.0f, "Steps", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_STEPS]->snapEnabled = true;

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");
		configButton(PARAM_RESET_TO_FIRST_STEP, "Reset to first step");
		configButton(PARAM_ONE_SHOT, "One shot");

		for (int i = 0; i < 8; i++) {
			configButton(PARAM_STEP1 + i, "Step " + std::to_string(i + 1));
			configOutput(OUTPUT_OUT1 + i, "Voltage " + std::to_string(i + 1));
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
		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);
		clockDivider.setDivision(16);
	};

	void doDecreaseTrigger() {
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			selectedOut--;
		else
		{
			selectedOut = stepCount - 1;
			bClockReceived = true;
		}
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	}

	void doIncreaseTrigger() {
		selectedOut++;
		bClockReceived = true;
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	};

	void doRandomTrigger() {
		if (!bNoRepeats) {
			selectedOut = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
		}
		else {
			randomNum = selectedOut;
			while (randomNum == selectedOut)
				randomNum = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
			selectedOut = randomNum;
		}
		bClockReceived = true;
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	}

	void doResetTrigger() {
		if (bResetToFirstStep)
			selectedOut = 0;
		else
		{
			channelCount = 0;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel += 4) {
				outVoltages[channel / 4] = 0.F;
			}

			for (int i = 0; i < 8; i++) {
				outputs[OUTPUT_OUT1 + i].setChannels(0);
			}
			selectedOut = -1;
			bClockReceived = false;
		}
		stepsDone = 0;
		if (bOneShot)
			bOneShotDone = false;
	};

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {
			if (inputs[INPUT_STEPS].isConnected())
				stepCount = round(clamp(inputs[INPUT_STEPS].getVoltage(), 1.0f, 8.0f));
			else if (params[PARAM_STEPS].getValue() != stepCount) {
				stepCount = params[PARAM_STEPS].getValue();
			}
		}

		if ((inputs[INPUT_RESET].isConnected() && stInputReset.process(inputs[INPUT_RESET].getVoltage()))
			|| btReset.process(params[PARAM_RESET].getValue())) {
			doResetTrigger();
		}

		for (int channel = 0; channel < PORT_MAX_CHANNELS; channel += 4) {
			outVoltages[channel / 4] = 0.f;
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
				for (int i = 0; i < stepCount; i++) {
					if (btSteps[i].process(params[PARAM_STEP1 + i].getValue())) {
						if (selectedOut > -1) {
							outputs[OUTPUT_OUT1 + selectedOut].setChannels(0);
						}
						selectedOut = i;
					}

					while (selectedOut < 0)
						selectedOut += stepCount;
					while (selectedOut >= stepCount)
						selectedOut -= stepCount;
				}

				if (inputs[INPUT_IN].isConnected()) {
					channelCount = inputs[INPUT_IN].getChannels();

					for (int channel = 0; channel < channelCount; channel += 4) {
						outVoltages[channel / 4] = inputs[INPUT_IN].getVoltageSimd<float_4>(channel);
					}
				}
			}

			for (int i = 0; i < 8; i++)
			{
				if (i < stepCount) {
					params[PARAM_STEP1 + i].setValue(i == selectedOut ? 1 : 0);
					if (i != selectedOut) {
						outputs[OUTPUT_OUT1 + i].setChannels(0);
					}
				}
				else
				{
					params[PARAM_STEP1 + i].setValue(2);
					outputs[OUTPUT_OUT1 + i].setChannels(0);
				}
			}

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				if (outputs[OUTPUT_OUT1 + selectedOut].isConnected()) {
					outputs[OUTPUT_OUT1 + selectedOut].setChannels(channelCount);
					for (int channel = 0; channel < channelCount; channel += 4) {
						outputs[OUTPUT_OUT1 + selectedOut].setVoltageSimd(outVoltages[channel / 4], channel);
					}
				}
			}

			if (bOneShot && stepsDone == stepCount)
				bOneShotDone = true;
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
		bResetToFirstStep = params[PARAM_RESET_TO_FIRST_STEP].getValue();
		if (!bLastResetToFirstStepValue && bResetToFirstStep)
			selectedOut = 0;
		bLastResetToFirstStepValue = bResetToFirstStep;
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && (bOneShot != bLastOneShotValue))
			bOneShotDone = false;
		bLastOneShotValue = bOneShot;
	};

	void onReset() override {
		if (bResetToFirstStep)
			selectedOut = 0;
		else {
			channelCount = 0;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel += 4) {
				outVoltages[channel / 4] = 0.F;
			}

			for (int i = 0; i < 8; i++) {
				outputs[OUTPUT_OUT1 + i].setChannels(0);
			}

			selectedOut = -1;
			bClockReceived = false;
		}
		stepCount = 8;
	}

	void onRandomize() override {
		stepCount = (int)pcg32_boundedrand_r(&pcgRng, 8) + 1;
		selectedOut = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "noRepeats", json_boolean(bNoRepeats));
		json_object_set_new(rootJ, "ResetToFirstStep", json_boolean(bResetToFirstStep));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* noRepeatsJ = json_object_get(rootJ, "noRepeats");
		if (noRepeatsJ)
			bNoRepeats = json_boolean_value(noRepeatsJ);

		json_t* ResetToFirstStepJ = json_object_get(rootJ, "ResetToFirstStep");
		bResetToFirstStep = json_boolean_value(ResetToFirstStepJ);
		bLastResetToFirstStepValue = bResetToFirstStep;
		if (!bResetToFirstStep) {
			selectedOut = -1;
			bClockReceived = false;
		}
		else
			selectedOut = 0;
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && bOneShot != bLastOneShotValue)
			bOneShotDone = false;
		bLastOneShotValue = bOneShot;
	}
};

struct SuperSwitch18Widget : SanguineModuleWidget {
	SuperSwitch18Widget(SuperSwitch18* module) {
		setModule(module);

		moduleName = "switch1-8";
		panelSize = SIZE_13;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

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

		currentY = 43.085f;

		addParam(createParam<SeqButtonNoRepeatsSmall>(millimetersToPixelsVec(28.711, currentY), module, SuperSwitch18::PARAM_NO_REPEATS));

		addParam(createParam<SeqButtonOneShotSmall>(millimetersToPixelsVec(15.844, currentY), module, SuperSwitch18::PARAM_ONE_SHOT));

		addParam(createParam<SeqButtonResetToOne>(millimetersToPixelsVec(2.977, currentY), module, SuperSwitch18::PARAM_RESET_TO_FIRST_STEP));

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_INCREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_DECREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.487, currentY), module, SuperSwitch18::INPUT_RESET));

		addParam(createParamCentered<SeqButtonUp>(millimetersToPixelsVec(25.349, 55.292), module, SuperSwitch18::PARAM_INCREASE));

		addParam(createParamCentered<SeqButtonDown>(millimetersToPixelsVec(25.349, 69.922), module, SuperSwitch18::PARAM_DECREASE));

		addParam(createParamCentered<SeqButtonRandom>(millimetersToPixelsVec(25.349, 84.553), module, SuperSwitch18::PARAM_RANDOM));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(25.349, 99.184), module, SuperSwitch18::PARAM_RESET));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(10.237, 116.012), module, SuperSwitch18::INPUT_IN));

		FramebufferWidget* switchFrameBuffer = new FramebufferWidget();
		addChild(switchFrameBuffer);

		SanguineLedNumberDisplay* display = new SanguineLedNumberDisplay(2, module, 26.644, 21.472);
		switchFrameBuffer->addChild(display);
		display->fallbackNumber = 8;

		if (module)
			display->values.numberValue = (&module->stepCount);

		SanguineStaticRGBLight* stepsLight = new SanguineStaticRGBLight(module, "res/seqs/light_steps.svg", 27.015, 34.372, true, kSanguineBlueLight);
		addChild(stepsLight);

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 10.237, 108.611);
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 53.4, 15.114);
		addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight(module, "res/blood_glowy_small.svg", 26.154, 112.707);
		addChild(bloodLogo);
	}
};

Model* modelSuperSwitch18 = createModel<SuperSwitch18, SuperSwitch18Widget>("Sanguine-SuperSwitch18");