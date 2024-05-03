#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"
#include "seqcomponents.hpp"

struct SuperSwitch18 : Module {

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
		PARAM_UP,
		PARAM_DOWN,
		PARAM_RANDOM,
		PARAM_RESET,
		PARAM_RESET_TO_FIRST_STEP,
		PARAM_ONE_SHOT,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_IN,
		INPUT_STEPS,
		INPUT_UP,
		INPUT_DOWN,
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

	dsp::SchmittTrigger stNoRepeats;
	dsp::SchmittTrigger stResetToFirstStep;
	dsp::SchmittTrigger stInputUp;
	dsp::SchmittTrigger stInputDown;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
	dsp::SchmittTrigger stDown;
	dsp::SchmittTrigger stUp;
	dsp::SchmittTrigger stRandom;
	dsp::SchmittTrigger stReset;
	dsp::SchmittTrigger stStep[8];
	bool bClockReceived = false;
	bool bLastResetToFirstStepValue = true;
	bool bNoRepeats = false;
	bool bOneShot = false;
	bool bResetToFirstStep = true;
	float outVoltages[8 * PORT_MAX_CHANNELS] = {};
	int outChannelsCounts[8] = {};
	int stepCount = 8;
	int selectedOut = 0;
	int randomNum;

	pcg32_random_t pcgRng;

	SuperSwitch18() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT,0);

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

		configButton(PARAM_UP, "Previous step");
		configButton(PARAM_DOWN, "Next step");
		configButton(PARAM_RANDOM, "Random step");
		configButton(PARAM_RESET, "Reset");

		configInput(INPUT_UP, "Trigger previous step");
		configInput(INPUT_DOWN, "Trigger next step");
		configInput(INPUT_RANDOM, "Trigger random");
		configInput(INPUT_RESET, "Trigger reset");
		configInput(INPUT_IN, "Voltage");
		params[PARAM_STEP1].setValue(1);
		params[PARAM_RESET_TO_FIRST_STEP].setValue(1);
		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);
	};

	void doDownTrigger() {
		selectedOut++;
		bClockReceived = true;
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
	}

	void doResetTrigger() {
		if (bResetToFirstStep)
			selectedOut = 0;
		else
		{
			memset(outVoltages, 0, 8 * PORT_MAX_CHANNELS * sizeof(float));
			memset(outChannelsCounts, 0, sizeof(int) * 8);
			for (int i = 0; i < 8; i++) {
				outputs[OUTPUT_OUT1 + i].setChannels(outChannelsCounts[i]);
				outputs[OUTPUT_OUT1 + i].writeVoltages(outVoltages + i * PORT_MAX_CHANNELS);
			}
			selectedOut = -1;
			bClockReceived = false;
		}
	};

	void doUpTrigger() {
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			selectedOut--;
		else
		{
			selectedOut = stepCount - 1;
			bClockReceived = true;
		}
	}

	void process(const ProcessArgs& args) override {
		memset(outVoltages, 0, 8 * PORT_MAX_CHANNELS * sizeof(float));

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

		if (inputs[INPUT_UP].isConnected()) {
			if (stInputUp.process(inputs[INPUT_UP].getVoltage())) {
				doUpTrigger();
			}
		}

		if (inputs[INPUT_DOWN].isConnected()) {
			if (stInputDown.process(inputs[INPUT_DOWN].getVoltage())) {
				doDownTrigger();
			}
		}

		if (inputs[INPUT_RANDOM].isConnected()) {
			if (stInputRandom.process(inputs[INPUT_RANDOM].getVoltage())) {
				doRandomTrigger();
			}
		}

		if (stReset.process(params[PARAM_RESET].getValue()))
		{
			doResetTrigger();
		}

		if (stUp.process(params[PARAM_UP].getValue())) {
			doUpTrigger();
		}

		if (stDown.process(params[PARAM_DOWN].getValue())) {
			doDownTrigger();
		}

		if (stRandom.process(params[PARAM_RANDOM].getValue())) {
			doRandomTrigger();
		}

		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			for (int i = 0; i < stepCount; i++) {
				if (stStep[i].process(params[PARAM_STEP1 + i].getValue()))
					selectedOut = i;

				while (selectedOut < 0)
					selectedOut += stepCount;
				while (selectedOut >= stepCount)
					selectedOut -= stepCount;

				if (inputs[INPUT_IN].isConnected()) {
					inputs[INPUT_IN].readVoltages(outVoltages + selectedOut * PORT_MAX_CHANNELS);
					outChannelsCounts[selectedOut] = inputs[INPUT_IN].getChannels();
				}
			}

		for (int i = 0; i < 8; i++)
		{
			params[PARAM_STEP1 + i].setValue(i == selectedOut ? 1 : 0);

			if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived)) {
				outputs[OUTPUT_OUT1 + i].setChannels(outChannelsCounts[i]);
				outputs[OUTPUT_OUT1 + i].writeVoltages(outVoltages + i * PORT_MAX_CHANNELS);
			}
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
		bResetToFirstStep = params[PARAM_RESET_TO_FIRST_STEP].getValue();
		if (!bLastResetToFirstStepValue && bResetToFirstStep)
			selectedOut = 0;
		bLastResetToFirstStepValue = bResetToFirstStep;
	};

	void onReset() override {
		if (bResetToFirstStep)
			selectedOut = 0;
		else {
			selectedOut = -1;
			bClockReceived = false;
		}
		stepCount = 8;
		memset(outChannelsCounts, 0, sizeof(int) * 8);
		memset(outVoltages, 0, 8 * PORT_MAX_CHANNELS * sizeof(float));
	}

	void onRandomize() override {
		stepCount = (int)pcg32_boundedrand_r(&pcgRng, 8) + 1;
		selectedOut = (int)pcg32_boundedrand_r(&pcgRng, stepCount);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "noRepeats", json_boolean(bNoRepeats));
		json_object_set_new(rootJ, "ResetToFirstStep", json_boolean(bResetToFirstStep));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
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
	}
};

struct SuperSwitch18Widget : ModuleWidget {
	SuperSwitch18Widget(SuperSwitch18* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/switch1-8.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(9.844, 18.272)), module, SuperSwitch18::PARAM_STEPS));
		addInput(createInputCentered<BananutBlack>(mm2px(Vec(9.844, 32.461)), module, SuperSwitch18::INPUT_STEPS));		

		float currentY = 23.904f;
		float deltaY = 13.129f;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT1));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT2));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT3));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT4));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT5));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT6));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT7));
		currentY += deltaY;
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(59.887, currentY)), module, SuperSwitch18::OUTPUT_OUT8));

		currentY = 20.104f;
		SeqStepSwitch* step1 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP1);
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_off.svg")));
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_on.svg")));
		addParam(step1);
		currentY += deltaY;
		SeqStepSwitch* step2 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP2);
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_off.svg")));
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_on.svg")));
		addParam(step2);
		currentY += deltaY;
		SeqStepSwitch* step3 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP3);
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_off.svg")));
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_on.svg")));
		addParam(step3);
		currentY += deltaY;
		SeqStepSwitch* step4 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP4);
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_off.svg")));
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_on.svg")));
		addParam(step4);
		currentY += deltaY;
		SeqStepSwitch* step5 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP5);
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_off.svg")));
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_on.svg")));
		addParam(step5);
		currentY += deltaY;
		SeqStepSwitch* step6 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP6);
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_off.svg")));
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_on.svg")));
		addParam(step6);
		currentY += deltaY;
		SeqStepSwitch* step7 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP7);
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_off.svg")));
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_on.svg")));
		addParam(step7);
		currentY += deltaY;
		SeqStepSwitch* step8 = createParam<SeqStepSwitch>(mm2px(Vec(39.677, currentY)),
			module, SuperSwitch18::PARAM_STEP8);
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_off.svg")));
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_on.svg")));
		addParam(step8);

		currentY = 43.085f;

		SanguineLightUpSwitch* switchNoRepeats = createParam<SanguineLightUpSwitch>(mm2px(Vec(28.711, currentY)),
			module, SuperSwitch18::PARAM_NO_REPEATS);
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_off.svg")));
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_on.svg")));
		switchNoRepeats->haloColorOff = nvgRGB(0, 0, 0);
		switchNoRepeats->haloColorOn = nvgRGB(206, 61, 255);
		switchNoRepeats->momentary = false;
		addParam(switchNoRepeats);

		SanguineLightUpSwitch* switchOneShot = createParam<SanguineLightUpSwitch>(mm2px(Vec(15.844, currentY)),
			module, SuperSwitch18::PARAM_ONE_SHOT);
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_off.svg")));
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_on.svg")));
		switchOneShot->haloColorOff = nvgRGB(0, 0, 0);
		switchOneShot->haloColorOn = nvgRGB(255, 11, 11);
		switchOneShot->momentary = false;
		addParam(switchOneShot);

		SanguineLightUpSwitch* switchResetToFirstStep = createParam<SanguineLightUpSwitch>(mm2px(Vec(2.977, currentY)),
			module, SuperSwitch18::PARAM_RESET_TO_FIRST_STEP);
		switchResetToFirstStep->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_to_one_off.svg")));
		switchResetToFirstStep->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_to_one_on.svg")));
		switchResetToFirstStep->haloColorOff = nvgRGB(0, 0, 0);
		switchResetToFirstStep->haloColorOn = nvgRGB(230, 230, 230);
		switchResetToFirstStep->momentary = false;
		addParam(switchResetToFirstStep);

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.487, currentY)), module, SuperSwitch18::INPUT_UP));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.487, currentY)), module, SuperSwitch18::INPUT_DOWN));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.487, currentY)), module, SuperSwitch18::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.487, currentY)), module, SuperSwitch18::INPUT_RESET));

		currentY = 51.376f;
		SeqControlSwitch* btnUp = createParamCentered<SeqControlSwitch>(mm2px(Vec(21.433, currentY)),
			module, SuperSwitch18::PARAM_UP);
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_off.svg")));
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_on.svg")));
		addParam(btnUp);
		currentY += deltaY;
		SeqControlSwitch* btnDown = createParamCentered<SeqControlSwitch>(mm2px(Vec(21.433, currentY)),
			module, SuperSwitch18::PARAM_DOWN);
		btnDown->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_off.svg")));
		btnDown->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_on.svg")));
		addParam(btnDown);
		currentY += deltaY;
		SeqControlSwitch* btnRandom = createParamCentered<SeqControlSwitch>(mm2px(Vec(21.433, currentY)),
			module, SuperSwitch18::PARAM_RANDOM);
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_off.svg")));
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_on.svg")));
		addParam(btnRandom);
		currentY += deltaY;
		SeqControlSwitch* btnReset = createParamCentered<SeqControlSwitch>(mm2px(Vec(21.433, currentY)),
			module, SuperSwitch18::PARAM_RESET);
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset);

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(10.237, 116.012)), module, SuperSwitch18::INPUT_IN));

		FramebufferWidget* switchFrameBuffer = new FramebufferWidget();
		addChild(switchFrameBuffer);

		SanguineLedNumberDisplay* display = new SanguineLedNumberDisplay();
		display->box.pos = mm2px(Vec(18.894, 13.972));
		display->box.size = mm2px(Vec(15.5, 15));
		display->module = module;
		display->textColor = nvgRGB(200, 0, 0);
		switchFrameBuffer->addChild(display);

		if (module)
			display->value = (&module->stepCount);

		SanguineShapedLight* stepsLight = new SanguineShapedLight();
		stepsLight->box.pos = mm2px(Vec(23.285, 30.505));
		stepsLight->wrap();
		stepsLight->module = module;
		stepsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/seqs/light_steps.svg")));
		switchFrameBuffer->addChild(stepsLight);	

		SanguineShapedLight* inLight = new SanguineShapedLight();
		inLight->box.pos = mm2px(Vec(6.943, 106.547));
		inLight->wrap();
		inLight->module = module;
		inLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/in_light.svg")));
		switchFrameBuffer->addChild(inLight);

		SanguineShapedLight* outLight = new SanguineShapedLight();
		outLight->box.pos = mm2px(Vec(50.106, 13.049));
		outLight->wrap();
		outLight->module = module;
		outLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/out_light.svg")));
		switchFrameBuffer->addChild(outLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = mm2px(Vec(25.038, 110.39));
		bloodLogo->box.size = mm2px(Vec(2.233, 4.634));
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_small.svg")));
		switchFrameBuffer->addChild(bloodLogo);
	}
};

Model* modelSuperSwitch18 = createModel<SuperSwitch18, SuperSwitch18Widget>("Sanguine-SuperSwitch18");