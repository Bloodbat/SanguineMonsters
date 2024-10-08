#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"
#include "pcg_variants.h"

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
	int inChannelCount = 0;
	int randomNum;
	int selectedIn = 0;
	int stepCount = 8;
	int stepsDone = 0;
	float_4 outVoltages[4] = {};

	dsp::ClockDivider clockDivider;

	pcg32_random_t pcgRng;

	SuperSwitch81() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, 0);

		configParam(PARAM_STEPS, 1.0f, 8.0f, 8.0f, "Steps", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_STEPS]->snapEnabled = true;

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");
		configButton(PARAM_RESET_TO_FIRST_STEP, "Reset to first step");
		configButton(PARAM_ONE_SHOT, "One shot");

		for (int i = 0; i < 8; i++) {
			configButton(PARAM_STEP1 + i, "Step " + std::to_string(i + 1));
			configInput(INPUT_IN1 + i, "Voltage " + std::to_string(i + 1));
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
		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);

		clockDivider.setDivision(16);
	};

	void doDecreaseTrigger() {
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			selectedIn--;
		else
		{
			selectedIn = stepCount - 1;
			bClockReceived = true;
		}
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	};

	void doIncreaseTrigger() {
		selectedIn++;
		bClockReceived = true;
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	};

	void doRandomTrigger() {
		if (!bNoRepeats) {
			selectedIn = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
		}
		else {
			randomNum = selectedIn;
			while (randomNum == selectedIn)
				randomNum = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
			selectedIn = randomNum;
		}
		bClockReceived = true;
		stepsDone++;
		if (stepsDone > stepCount)
			stepsDone = 0;
	};

	void doResetTrigger() {
		if (bResetToFirstStep)
			selectedIn = 0;
		else {
			selectedIn = -1;
			bClockReceived = false;
			outputs[OUTPUT_OUT].setChannels(0);
			for (int i = 0; i < PORT_MAX_CHANNELS; i += 4) {
				outVoltages[i / 4] = 0.f;
			}
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

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
				for (int i = 0; i < stepCount; i++) {
					if (btSteps[i].process(params[PARAM_STEP1 + i].getValue()))
						selectedIn = i;

					while (selectedIn < 0)
						selectedIn += stepCount;
					while (selectedIn >= stepCount)
						selectedIn -= stepCount;

					if (inputs[INPUT_IN1 + selectedIn].isConnected()) {
						inChannelCount = inputs[INPUT_IN1 + selectedIn].getChannels();
						for (int channel = 0; channel < inChannelCount; channel += 4) {
							outVoltages[channel / 4] = inputs[INPUT_IN1 + selectedIn].getVoltageSimd<float_4>(channel);
						}
					}
				}

			for (int i = 0; i < 8; i++) {
				if (i < stepCount) {
					params[PARAM_STEP1 + i].setValue(i == selectedIn ? 1 : 0);
				}
				else
				{
					params[PARAM_STEP1 + i].setValue(2);
				}
			}

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				if (outputs[OUTPUT_OUT].isConnected()) {
					outputs[OUTPUT_OUT].setChannels(inChannelCount);
					for (int channel = 0; channel < inChannelCount; channel += 4) {
						outputs[OUTPUT_OUT].setVoltageSimd(outVoltages[channel / 4], channel);
					}
				}

				if (bOneShot && stepsDone == stepCount)
					bOneShotDone = true;
			}
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
		bResetToFirstStep = params[PARAM_RESET_TO_FIRST_STEP].getValue();
		if (!bLastResetToFirstStepValue && bResetToFirstStep)
			selectedIn = 0;
		bLastResetToFirstStepValue = bResetToFirstStep;
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && (bOneShot != bLastOneShotValue))
			bOneShotDone = false;
		bLastOneShotValue = bOneShot;
	};

	void onReset() override {
		if (bResetToFirstStep)
			selectedIn = 0;
		else {
			selectedIn = -1;
			bClockReceived = false;
		}
		stepCount = 8;
	}

	void onRandomize() override {
		stepCount = (int)pcg32_boundedrand_r(&pcgRng, 8) + 1;
		selectedIn = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "noRepeats", json_boolean(bNoRepeats));
		json_object_set_new(rootJ, "resetToFirstStep", json_boolean(bResetToFirstStep));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* noRepeatsJ = json_object_get(rootJ, "noRepeats");
		if (noRepeatsJ)
			bNoRepeats = json_boolean_value(noRepeatsJ);

		json_t* resetToFirstStepJ = json_object_get(rootJ, "resetToFirstStep");
		bResetToFirstStep = json_boolean_value(resetToFirstStepJ);
		bLastResetToFirstStepValue = bResetToFirstStep;
		if (!bResetToFirstStep) {
			selectedIn = -1;
			bClockReceived = false;
		}
		else
			selectedIn = 0;
		bOneShot = params[PARAM_ONE_SHOT].getValue();
		if (bOneShot && bOneShot != bLastOneShotValue)
			bOneShotDone = false;
		bLastOneShotValue = bOneShot;
	}
};

struct SuperSwitch81Widget : SanguineModuleWidget {
	SuperSwitch81Widget(SuperSwitch81* module) {
		setModule(module);

		moduleName = "switch8-1";
		panelSize = SIZE_13;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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
		display->fallbackNumber = 8;

		if (module)
			display->values.numberValue = (&module->stepCount);

		SanguineStaticRGBLight* stepsLight = new SanguineStaticRGBLight(module, "res/seqs/light_steps.svg", 39.026, 34.372, true, kSanguineBlueLight);
		addChild(stepsLight);

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 13.041, 15.714);
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 55.803, 108.311);
		addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight(module, "res/blood_glowy_small.svg", 35.585, 112.707);
		addChild(bloodLogo);
	}
};

Model* modelSuperSwitch81 = createModel<SuperSwitch81, SuperSwitch81Widget>("Sanguine-SuperSwitch81");
