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
		PARAM_UP,
		PARAM_DOWN,
		PARAM_RANDOM,
		PARAM_RESET,
		PARAM_RESET_TO_FIRST_STEP,
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
		INPUT_UP,
		INPUT_DOWN,
		INPUT_RANDOM,
		INPUT_RESET,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT,
		OUTPUTS_COUNT
	};

	enum LightId {
		LIGHT_NO_REPEATS,
		LIGHT_RESET_TO_FIRST_STEP,
		LIGHTS_COUNT
	};

	dsp::BooleanTrigger btNoRepeats;
	dsp::BooleanTrigger btResetToFirstStep;
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
	bool bNoRepeats = false;
	bool bResetToFirstStep = true;
	int inChannelCount = 0;
	int stepCount = 8;
	int selectedIn = 0;
	int randomNum;
	float outVoltages[PORT_MAX_CHANNELS];

	pcg32_random_t pcgRng;

	SuperSwitch81() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_STEPS, 1.0f, 8.0f, 8.0f, "Steps", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_STEPS]->snapEnabled = true;

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");
		configButton(PARAM_RESET_TO_FIRST_STEP, "Reset to first step");

		for (int i = 0; i < 8; i++) {
			configButton(PARAM_STEP1 + i, "Step " + std::to_string(i + 1));
			configInput(INPUT_IN1 + i, "Voltage " + std::to_string(i + 1));
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
		configOutput(OUTPUT_OUT, "Voltage");
		params[PARAM_STEP1].setValue(1);
		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);
	};

	void doDownTrigger() {
		selectedIn++;
		bClockReceived = true;
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
	};

	void doResetTrigger() {
		if (bResetToFirstStep)
			selectedIn = 0;
		else {
			selectedIn = -1;
			bClockReceived = false;
			// TODO!! BUG!!  Stop sending value when resetting!
		}
	};

	void doUpTrigger() {
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			selectedIn--;
		else
		{
			selectedIn = stepCount - 1;
			bClockReceived = true;
		}
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

		outputs[OUTPUT_OUT].setChannels(inChannelCount);
		if (bResetToFirstStep || (!bResetToFirstStep && bClockReceived))
			outputs[OUTPUT_OUT].writeVoltages(outVoltages);

		if (btNoRepeats.process(params[PARAM_NO_REPEATS].getValue()))
			bNoRepeats ^= true;

		if (btResetToFirstStep.process(params[PARAM_RESET_TO_FIRST_STEP].getValue())) {
			bResetToFirstStep ^= true;
			if (bResetToFirstStep)
				selectedIn = 0;
		}

		lights[LIGHT_NO_REPEATS].setBrightnessSmooth(bNoRepeats, args.sampleTime);
		lights[LIGHT_RESET_TO_FIRST_STEP].setBrightnessSmooth(bResetToFirstStep, args.sampleTime);
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
		if (!bResetToFirstStep) {
			selectedIn = -1;
			bClockReceived = false;
		}
		else
			selectedIn = 0;
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

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(mm2px(Vec(40.928, 45.085)),
			module, SuperSwitch81::PARAM_NO_REPEATS, SuperSwitch81::LIGHT_NO_REPEATS));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<BlueLight>>>(mm2px(Vec(51.759, 45.085)),
			module, SuperSwitch81::PARAM_RESET_TO_FIRST_STEP, SuperSwitch81::LIGHT_RESET_TO_FIRST_STEP));

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

		currentY = 55.291f;
		deltaY = 14.631f;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_UP));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_DOWN));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_RANDOM));
		currentY += deltaY;
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(59.553, currentY)), module, SuperSwitch81::INPUT_RESET));

		currentY = 51.376f;
		SeqControlSwitch* btnUp = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_UP);
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_off.svg")));
		btnUp->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_on.svg")));
		addParam(btnUp);
		currentY += deltaY;
		SeqControlSwitch* btnDown = createParamCentered<SeqControlSwitch>(mm2px(Vec(36.776, currentY)),
			module, SuperSwitch81::PARAM_DOWN);
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
		display->box.size = mm2px(Vec(15.5, 15));
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

		SanguineShapedLight* resetToFirstStepLight = new SanguineShapedLight();
		resetToFirstStepLight->box.pos = mm2px(Vec(56.723, 42.74));
		resetToFirstStepLight->wrap();
		resetToFirstStepLight->module = module;
		resetToFirstStepLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/seqs/from_one.svg")));
		switchFrameBuffer->addChild(resetToFirstStepLight);

		SanguineShapedLight* entropyLight = new SanguineShapedLight();
		entropyLight->box.pos = mm2px(Vec(31.011, 42.982));
		entropyLight->wrap();
		entropyLight->module = module;
		entropyLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/seqs/uranus_glyph.svg")));
		switchFrameBuffer->addChild(entropyLight);

		SanguineShapedLight* inLight = new SanguineShapedLight();
		inLight->box.pos = mm2px(Vec(9.347, 13.649));
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
		bloodLogo->box.pos = mm2px(Vec(34.468, 110.39));
		bloodLogo->box.size = mm2px(Vec(2.233, 4.634));
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_small.svg")));
		switchFrameBuffer->addChild(bloodLogo);
	}
};

Model* modelSuperSwitch81 = createModel<SuperSwitch81, SuperSwitch81Widget>("Sanguine-SuperSwitch81");
