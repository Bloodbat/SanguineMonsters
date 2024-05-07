#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"
#include "seqcomponents.hpp"

struct SuperSwitch81 : Module {

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

	dsp::SchmittTrigger stDecrease;
	dsp::SchmittTrigger stIncrease;
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
	dsp::SchmittTrigger stNoRepeats;
	dsp::SchmittTrigger stRandom;
	dsp::SchmittTrigger stReset;
	dsp::SchmittTrigger stResetToFirstStep;
	dsp::SchmittTrigger stStep[8];
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
	float outVoltages[PORT_MAX_CHANNELS];

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
			memset(outVoltages, 0, PORT_MAX_CHANNELS * sizeof(float));
			outputs[OUTPUT_OUT].setChannels(0);
			outputs[OUTPUT_OUT].writeVoltages(outVoltages);
		}
		stepsDone = 0;
		if (bOneShot)
			bOneShotDone = false;
	};

	void process(const ProcessArgs& args) override {
		memset(outVoltages, 0, PORT_MAX_CHANNELS * sizeof(float));

		if (inputs[INPUT_STEPS].isConnected())
			stepCount = round(clamp(inputs[INPUT_STEPS].getVoltage(), 1.0f, 8.0f));
		else if (params[PARAM_STEPS].getValue() != stepCount) {
			stepCount = params[PARAM_STEPS].getValue();
		}

		if (inputs[INPUT_RESET].isConnected()) {
			if (stInputReset.process(inputs[INPUT_RESET].getVoltage())) {
				doResetTrigger();
			}
		}

		if (stReset.process(params[PARAM_RESET].getValue()))
		{
			doResetTrigger();
		}

		if (!bOneShot || (bOneShot && !bOneShotDone)) {
			if (inputs[INPUT_DECREASE].isConnected()) {
				if (stInputDecrease.process(inputs[INPUT_DECREASE].getVoltage())) {
					doDecreaseTrigger();
				}
			}

			if (inputs[INPUT_INCREASE].isConnected()) {
				if (stInputIncrease.process(inputs[INPUT_INCREASE].getVoltage())) {
					doIncreaseTrigger();
				}
			}

			if (inputs[INPUT_RANDOM].isConnected()) {
				if (stInputRandom.process(inputs[INPUT_RANDOM].getVoltage())) {
					doRandomTrigger();
				}
			}

			if (stDecrease.process(params[PARAM_DECREASE].getValue())) {
				doDecreaseTrigger();
			}

			if (stIncrease.process(params[PARAM_INCREASE].getValue())) {
				doIncreaseTrigger();
			}

			if (stRandom.process(params[PARAM_RANDOM].getValue())) {
				doRandomTrigger();
			}

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
				for (int i = 0; i < stepCount; i++) {
					if (stStep[i].process(params[PARAM_STEP1 + i].getValue()))
						selectedIn = i;

					while (selectedIn < 0)
						selectedIn += stepCount;
					while (selectedIn >= stepCount)
						selectedIn -= stepCount;

					if (inputs[INPUT_IN1 + selectedIn].isConnected()) {
						inChannelCount = inputs[INPUT_IN1 + selectedIn].getChannels();
						inputs[INPUT_IN1 + selectedIn].readVoltages(outVoltages);
					}
				}

			for (int i = 0; i < 8; i++)
				params[PARAM_STEP1 + i].setValue(i == selectedIn ? 1 : 0);

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				outputs[OUTPUT_OUT].setChannels(inChannelCount);
				outputs[OUTPUT_OUT].writeVoltages(outVoltages);

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
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "noRepeats", json_boolean(bNoRepeats));
		json_object_set_new(rootJ, "resetToFirstStep", json_boolean(bResetToFirstStep));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
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

struct SuperSwitch81Widget : ModuleWidget {
	SuperSwitch81Widget(SuperSwitch81* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/switch8-1.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(56.197, 18.272)), module, SuperSwitch81::PARAM_STEPS));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(56.197, 32.461)), module, SuperSwitch81::INPUT_STEPS));

		float currentY = 23.904f;
		float deltaY = 13.129f;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN1));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN2));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN3));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN4));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN5));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN6));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN7));
		currentY += deltaY;
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.153, currentY)), module, SuperSwitch81::INPUT_IN8));

		currentY = 20.104f;
		SeqStepSwitch* step1 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP1);
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_off.svg")));
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_on.svg")));
		addParam(step1);
		currentY += deltaY;
		SeqStepSwitch* step2 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP2);
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_off.svg")));
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_on.svg")));
		addParam(step2);
		currentY += deltaY;
		SeqStepSwitch* step3 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP3);
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_off.svg")));
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_on.svg")));
		addParam(step3);
		currentY += deltaY;
		SeqStepSwitch* step4 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP4);
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_off.svg")));
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_on.svg")));
		addParam(step4);
		currentY += deltaY;
		SeqStepSwitch* step5 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP5);
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_off.svg")));
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_on.svg")));
		addParam(step5);
		currentY += deltaY;
		SeqStepSwitch* step6 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP6);
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_off.svg")));
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_on.svg")));
		addParam(step6);
		currentY += deltaY;
		SeqStepSwitch* step7 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP7);
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_off.svg")));
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_on.svg")));
		addParam(step7);
		currentY += deltaY;
		SeqStepSwitch* step8 = createParam<SeqStepSwitch>(mm2px(Vec(18.763, currentY)),
			module, SuperSwitch81::PARAM_STEP8);
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_off.svg")));
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_on.svg")));
		addParam(step8);

		currentY = 43.085f;

		SanguineLightUpSwitch* switchNoRepeats = createParam<SanguineLightUpSwitch>(mm2px(Vec(33.4, currentY)),
			module, SuperSwitch81::PARAM_NO_REPEATS);
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_off.svg")));
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_on.svg")));
		switchNoRepeats->haloColorOff = nvgRGB(0, 0, 0);
		switchNoRepeats->haloColorOn = nvgRGB(206, 61, 255);
		switchNoRepeats->momentary = false;
		addParam(switchNoRepeats);

		SanguineLightUpSwitch* switchOneShot = createParam<SanguineLightUpSwitch>(mm2px(Vec(46.3, currentY)),
			module, SuperSwitch81::PARAM_ONE_SHOT);
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_off.svg")));
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_on.svg")));
		switchOneShot->haloColorOff = nvgRGB(0, 0, 0);
		switchOneShot->haloColorOn = nvgRGB(255, 11, 11);
		switchOneShot->momentary = false;
		addParam(switchOneShot);

		SanguineLightUpSwitch* switchResetToFirstStep = createParam<SanguineLightUpSwitch>(mm2px(Vec(59.2, currentY)),
			module, SuperSwitch81::PARAM_RESET_TO_FIRST_STEP);
		switchResetToFirstStep->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_to_one_off.svg")));
		switchResetToFirstStep->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_to_one_on.svg")));
		switchResetToFirstStep->haloColorOff = nvgRGB(0, 0, 0);
		switchResetToFirstStep->haloColorOn = nvgRGB(230, 230, 230);
		switchResetToFirstStep->momentary = false;
		addParam(switchResetToFirstStep);

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_INCREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_DECREASE));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_RESET));

		currentY = 51.376f;
		SeqControlSwitch* btnUp = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_INCREASE);
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_off.svg")));
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_on.svg")));
		addParam(btnUp);
		currentY += deltaY;
		SeqControlSwitch* btnDown = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_DECREASE);
		btnDown->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_off.svg")));
		btnDown->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_on.svg")));
		addParam(btnDown);
		currentY += deltaY;
		SeqControlSwitch* btnRandom = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_RANDOM);
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_off.svg")));
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_on.svg")));
		addParam(btnRandom);
		currentY += deltaY;
		SeqControlSwitch* btnReset = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_RESET);
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(55.803, 116.012)), module, SuperSwitch81::OUTPUT_OUT));

		FramebufferWidget* switchFrameBuffer = new FramebufferWidget();
		addChild(switchFrameBuffer);

		SanguineLedNumberDisplay* display = new SanguineLedNumberDisplay();
		display->box.pos = mm2px(Vec(31.647, 13.972));		
		display->module = module;
		display->textColor = nvgRGB(200, 0, 0);
		switchFrameBuffer->addChild(display);

		if (module)
			display->value = (&module->stepCount);

		SanguineShapedLight* stepsLight = new SanguineShapedLight();
		stepsLight->box.pos = mm2px(Vec(35.296, 30.505));
		stepsLight->wrap();
		stepsLight->module = module;
		stepsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/seqs/light_steps.svg")));
		switchFrameBuffer->addChild(stepsLight);

		SanguineShapedLight* inLight = new SanguineShapedLight();
		inLight->box.pos = Vec(27.78, 40.36);
		inLight->wrap();
		inLight->module = module;
		inLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/in_light.svg")));
		switchFrameBuffer->addChild(inLight);

		SanguineShapedLight* outLight = new SanguineShapedLight();
		outLight->box.pos = mm2px(Vec(52.509, 106.247));
		outLight->wrap();
		outLight->module = module;
		outLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/out_light.svg")));
		switchFrameBuffer->addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = Vec(102.20, 326);
		bloodLogo->wrap();
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_small.svg")));
		switchFrameBuffer->addChild(bloodLogo);
	}
};

Model* modelSuperSwitch81 = createModel<SuperSwitch81, SuperSwitch81Widget>("Sanguine-SuperSwitch81");
