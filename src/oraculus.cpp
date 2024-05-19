#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"
#include "seqcomponents.hpp"

struct Oraculus : Module {

	enum ParamIds {
		PARAM_INCREASE,
		PARAM_DECREASE,
		PARAM_RANDOM,
		PARAM_RESET,
		PARAM_NO_REPEATS,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_POLYPHONIC,
		INPUT_CV_OFFSET,
		INPUT_INCREASE,
		INPUT_DECREASE,
		INPUT_RANDOM,
		INPUT_RESET,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_MONOPHONIC,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_CHANNEL, 16 * 3),
		LIGHTS_COUNT
	};

	dsp::ClockDivider clockDivider;

	dsp::SchmittTrigger stDecrease;
	dsp::SchmittTrigger stIncrease;
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;
	dsp::SchmittTrigger stRandom;
	dsp::SchmittTrigger stReset;

	int channelCount = 0;
	int finalChannel = -1;
	int selectedChannel = 0;

	bool bCvConnected = false;
	bool bDecreaseConnected = false;
	bool bIncreaseConnected = false;
	bool bNoRepeats = false;
	bool bOutputConnected = false;
	bool bRandomConnected = false;
	bool bResetConnected = false;

	const static int kClockUpdateFrequency = 16;

	pcg32_random_t pcgRng;

	Oraculus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_NO_REPEATS, "No random consecutive repeats");

		configButton(PARAM_DECREASE, "Previous step");
		configButton(PARAM_INCREASE, "Next step");
		configButton(PARAM_RANDOM, "Random step");
		configButton(PARAM_RESET, "Reset");

		configInput(INPUT_DECREASE, "Trigger previous step");
		configInput(INPUT_INCREASE, "Trigger next step");
		configInput(INPUT_RANDOM, "Trigger random");
		configInput(INPUT_RESET, "Trigger reset");

		configInput(INPUT_POLYPHONIC, "Polyphonic");
		configInput(INPUT_CV_OFFSET, "Channel select offset CV");

		configOutput(OUTPUT_MONOPHONIC, "Monophonic");

		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);

		clockDivider.setDivision(kClockUpdateFrequency);
		onReset();
	};

	void doDecreaseTrigger() {
		selectedChannel = ((selectedChannel - 1) + channelCount) % channelCount;
	};

	void doIncreaseTrigger() {
		selectedChannel = (selectedChannel + 1) % channelCount;
	};

	void doRandomTrigger() {
		int randomNum;
		if (!bNoRepeats) {
			selectedChannel = (int)pcg32_boundedrand_r(&pcgRng, channelCount);
		}
		else {
			randomNum = selectedChannel;
			while (randomNum == selectedChannel)
				randomNum = (int)pcg32_boundedrand_r(&pcgRng, channelCount);
			selectedChannel = randomNum;
		}
	};

	void doResetTrigger() {
		selectedChannel = 0;
	};

	void process(const ProcessArgs& args) override {
		channelCount = inputs[INPUT_POLYPHONIC].getChannels();

		if (clockDivider.process()) {
			checkConnections();
			updateLights();
		}

		if (bResetConnected && stInputReset.process(inputs[INPUT_RESET].getVoltage())) {
			doResetTrigger();
		}

		if (stReset.process(params[PARAM_RESET].getValue()))
		{
			doResetTrigger();
		}

		if (channelCount > 0) {
			if ((bIncreaseConnected && stInputIncrease.process(inputs[INPUT_INCREASE].getVoltage()))
				|| stIncrease.process(params[PARAM_INCREASE].getValue())) {
				doIncreaseTrigger();
			}

			if ((bDecreaseConnected && stInputDecrease.process(inputs[INPUT_DECREASE].getVoltage()))
				|| stDecrease.process(params[PARAM_DECREASE].getValue())) {
				doDecreaseTrigger();
			}

			if ((bRandomConnected && stInputRandom.process(inputs[INPUT_RANDOM].getVoltage()))
				|| stRandom.process(params[PARAM_RANDOM].getValue())) {
				doRandomTrigger();
			}

			if (bCvConnected && channelCount > 1) {
				float cv = inputs[INPUT_CV_OFFSET].getVoltage();
				int channelOffset = std::floor(cv * (channelCount / 10.f));
				finalChannel = selectedChannel + channelOffset;
				if (finalChannel < 0)
					finalChannel = channelCount + channelOffset;
				if (finalChannel >= channelCount)
					finalChannel = channelOffset - (channelCount - selectedChannel);
			}
			else {
				finalChannel = selectedChannel;
			}


			if (channelCount < 1)
				finalChannel = -1;

			if (finalChannel > -1) {
				if (bOutputConnected) {
					outputs[OUTPUT_MONOPHONIC].setVoltage(inputs[INPUT_POLYPHONIC].getVoltage(finalChannel));
				}
			}
		}
		else {
			finalChannel = -1;
			if (bOutputConnected) {
				outputs[OUTPUT_MONOPHONIC].setChannels(0);
			}
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
	}

	void checkConnections() {
		bCvConnected = inputs[INPUT_CV_OFFSET].isConnected();
		bIncreaseConnected = inputs[INPUT_INCREASE].isConnected();
		bDecreaseConnected = inputs[INPUT_DECREASE].isConnected();
		bRandomConnected = inputs[INPUT_RANDOM].isConnected();
		bResetConnected = inputs[INPUT_RESET].isConnected();
		bOutputConnected = outputs[OUTPUT_MONOPHONIC].isConnected();
	}

	void updateLights() {
		// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
		const float sampleTime = APP->engine->getSampleTime() * kClockUpdateFrequency;

		int currentLight;
		for (int i = 0; i < 16; i++) {
			currentLight = i * 3;
			if (i == finalChannel) {
				lights[currentLight + 0].setBrightnessSmooth(0.59f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(1.f, sampleTime);
			}
			else if (i < channelCount) {
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.28f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.15f, sampleTime);
			}
			else {
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
		}
	}
};

struct OraculusWidget : ModuleWidget {
	OraculusWidget(Oraculus* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/oraculus.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(17.78, 18.643)), module, Oraculus::INPUT_POLYPHONIC));

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(6.452, 38.204)), module, Oraculus::INPUT_CV_OFFSET));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.452, 55.801)), module, Oraculus::INPUT_INCREASE));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.452, 68.984)), module, Oraculus::INPUT_DECREASE));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.452, 82.168)), module, Oraculus::INPUT_RANDOM));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(6.452, 95.351)), module, Oraculus::INPUT_RESET));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(17.78, 113.488)), module, Oraculus::OUTPUT_MONOPHONIC));

		SeqControlSwitch* btnIncrease = createParam<SeqControlSwitch>(mm2px(Vec(21.536, 51.886)),
			module, Oraculus::PARAM_INCREASE);
		btnIncrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_off.svg")));
		btnIncrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_on.svg")));
		addParam(btnIncrease);
		SeqControlSwitch* btnDecrease = createParam<SeqControlSwitch>(mm2px(Vec(21.5361, 65.069)),
			module, Oraculus::PARAM_DECREASE);
		btnDecrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_off.svg")));
		btnDecrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_on.svg")));
		addParam(btnDecrease);
		SeqControlSwitch* btnRandom = createParam<SeqControlSwitch>(mm2px(Vec(21.536, 78.253)),
			module, Oraculus::PARAM_RANDOM);
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_off.svg")));
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_on.svg")));
		addParam(btnRandom);
		SeqControlSwitch* btnReset = createParam<SeqControlSwitch>(mm2px(Vec(21.536, 91.436)),
			module, Oraculus::PARAM_RESET);
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset);

		FramebufferWidget* oraculusFrameBuffer = new FramebufferWidget();
		addChild(oraculusFrameBuffer);

		SanguineShapedLight* inMonoLight1 = new SanguineShapedLight();
		inMonoLight1->box.pos = mm2px(Vec(14.486, 9.656));
		inMonoLight1->wrap();
		inMonoLight1->module = module;
		inMonoLight1->setSvg(Svg::load(asset::plugin(pluginInstance, "res/in_light.svg")));
		oraculusFrameBuffer->addChild(inMonoLight1);

		SanguineShapedLight* outPolyLight1 = new SanguineShapedLight();
		outPolyLight1->box.pos = mm2px(Vec(14.486, 104.5));
		outPolyLight1->wrap();
		outPolyLight1->module = module;
		outPolyLight1->setSvg(Svg::load(asset::plugin(pluginInstance, "res/out_mono_light.svg")));
		oraculusFrameBuffer->addChild(outPolyLight1);

		SanguineLightUpSwitch* switchNoRepeats = createParam<SanguineLightUpSwitch>(mm2px(Vec(30.004, 20.21)),
			module, Oraculus::PARAM_NO_REPEATS);
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_off.svg")));
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_on.svg")));
		switchNoRepeats->haloColorOff = nvgRGB(0, 0, 0);
		switchNoRepeats->haloColorOn = nvgRGB(206, 61, 255);
		switchNoRepeats->momentary = false;
		addParam(switchNoRepeats);

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(27.431, 30.486)), module, Oraculus::LIGHT_CHANNEL + 0 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(30.141, 32.297)), module, Oraculus::LIGHT_CHANNEL + 1 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(31.952, 35.007)), module, Oraculus::LIGHT_CHANNEL + 2 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(32.588, 38.204)), module, Oraculus::LIGHT_CHANNEL + 3 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(31.952, 41.401)), module, Oraculus::LIGHT_CHANNEL + 4 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(30.141, 44.111)), module, Oraculus::LIGHT_CHANNEL + 5 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(27.431, 45.922)), module, Oraculus::LIGHT_CHANNEL + 6 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(24.234, 46.558)), module, Oraculus::LIGHT_CHANNEL + 7 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(21.037, 45.922)), module, Oraculus::LIGHT_CHANNEL + 8 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(18.327, 44.111)), module, Oraculus::LIGHT_CHANNEL + 9 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(16.516, 41.401)), module, Oraculus::LIGHT_CHANNEL + 10 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(15.88, 38.204)), module, Oraculus::LIGHT_CHANNEL + 11 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(16.516, 35.007)), module, Oraculus::LIGHT_CHANNEL + 12 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(18.327, 32.297)), module, Oraculus::LIGHT_CHANNEL + 13 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(21.037, 30.486)), module, Oraculus::LIGHT_CHANNEL + 14 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(24.234, 29.85)), module, Oraculus::LIGHT_CHANNEL + 15 * 3));
	}
};

Model* modelOraculus = createModel<Oraculus, OraculusWidget>("Sanguine-Monsters-Oraculus");