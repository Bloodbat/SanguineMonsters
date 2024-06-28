#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"

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

	dsp::BooleanTrigger btDecrease;
	dsp::BooleanTrigger btIncrease;
	dsp::BooleanTrigger btRandom;
	dsp::BooleanTrigger btReset;
	dsp::SchmittTrigger stInputDecrease;
	dsp::SchmittTrigger stInputIncrease;
	dsp::SchmittTrigger stInputRandom;
	dsp::SchmittTrigger stInputReset;

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
			updateLights(args);
		}

		if (bResetConnected && stInputReset.process(inputs[INPUT_RESET].getVoltage())) {
			doResetTrigger();
		}

		if (btReset.process(params[PARAM_RESET].getValue()))
		{
			doResetTrigger();
		}

		if (channelCount > 0) {
			if ((bIncreaseConnected && stInputIncrease.process(inputs[INPUT_INCREASE].getVoltage()))
				|| btIncrease.process(params[PARAM_INCREASE].getValue())) {
				doIncreaseTrigger();
			}

			if ((bDecreaseConnected && stInputDecrease.process(inputs[INPUT_DECREASE].getVoltage()))
				|| btDecrease.process(params[PARAM_DECREASE].getValue())) {
				doDecreaseTrigger();
			}

			if ((bRandomConnected && stInputRandom.process(inputs[INPUT_RANDOM].getVoltage()))
				|| btRandom.process(params[PARAM_RANDOM].getValue())) {
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

	void updateLights(const ProcessArgs& args) {
		// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
		const float sampleTime = args.sampleTime * kClockUpdateFrequency;

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

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_7hp_purple.svg", "res/oraculus.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(6.452, 23.464), module, Oraculus::INPUT_POLYPHONIC));

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(22.734, 43.189), module, Oraculus::INPUT_CV_OFFSET));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 55.801), module, Oraculus::INPUT_INCREASE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 68.984), module, Oraculus::INPUT_DECREASE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 82.168), module, Oraculus::INPUT_RANDOM));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 95.351), module, Oraculus::INPUT_RESET));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.78, 113.488), module, Oraculus::OUTPUT_MONOPHONIC));

		FramebufferWidget* oraculusFrameBuffer = new FramebufferWidget();
		addChild(oraculusFrameBuffer);

		SeqControlSwitch* btnIncrease = createParam<SeqControlSwitch>(millimetersToPixelsVec(21.536, 51.886),
			module, Oraculus::PARAM_INCREASE);
		btnIncrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_off.svg")));
		btnIncrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/up_on.svg")));
		oraculusFrameBuffer->addChild(btnIncrease);

		SeqControlSwitch* btnDecrease = createParam<SeqControlSwitch>(millimetersToPixelsVec(21.5361, 65.069),
			module, Oraculus::PARAM_DECREASE);
		btnDecrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_off.svg")));
		btnDecrease->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/down_on.svg")));
		oraculusFrameBuffer->addChild(btnDecrease);

		SeqControlSwitch* btnRandom = createParam<SeqControlSwitch>(millimetersToPixelsVec(21.536, 78.253),
			module, Oraculus::PARAM_RANDOM);
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_off.svg")));
		btnRandom->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/random_on.svg")));
		oraculusFrameBuffer->addChild(btnRandom);

		SeqControlSwitch* btnReset = createParam<SeqControlSwitch>(millimetersToPixelsVec(21.536, 91.436),
			module, Oraculus::PARAM_RESET);
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		oraculusFrameBuffer->addChild(btnReset);

		SanguinePolyInputLight* InPolyLight = new SanguinePolyInputLight();
		InPolyLight->box.pos = millimetersToPixelsVec(3.158, 12.721);
		InPolyLight->module = module;
		addChild(InPolyLight);

		SanguineMonoOutputLight* outMonoLight = new SanguineMonoOutputLight();
		outMonoLight->box.pos = millimetersToPixelsVec(14.486, 104.5);
		outMonoLight->module = module;
		addChild(outMonoLight);

		SanguineLightUpSwitch* switchNoRepeats = createParam<SanguineLightUpSwitch>(millimetersToPixelsVec(9.454, 41.189),
			module, Oraculus::PARAM_NO_REPEATS);
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_off.svg")));
		switchNoRepeats->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/no_repeats_on.svg")));
		switchNoRepeats->addHalo(nvgRGB(0, 0, 0));
		switchNoRepeats->addHalo(nvgRGB(206, 61, 255));
		switchNoRepeats->momentary = false;
		oraculusFrameBuffer->addChild(switchNoRepeats);

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.734, 15.11), module, Oraculus::LIGHT_CHANNEL + 0 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(26.931, 15.746), module, Oraculus::LIGHT_CHANNEL + 1 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(29.641, 17.557), module, Oraculus::LIGHT_CHANNEL + 2 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(31.452, 20.267), module, Oraculus::LIGHT_CHANNEL + 3 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.088, 23.464), module, Oraculus::LIGHT_CHANNEL + 4 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(31.452, 26.661), module, Oraculus::LIGHT_CHANNEL + 5 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(29.641, 29.372), module, Oraculus::LIGHT_CHANNEL + 6 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(26.931, 31.182), module, Oraculus::LIGHT_CHANNEL + 7 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.734, 31.818), module, Oraculus::LIGHT_CHANNEL + 8 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.537, 31.182), module, Oraculus::LIGHT_CHANNEL + 9 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.827, 29.372), module, Oraculus::LIGHT_CHANNEL + 10 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.016, 26.661), module, Oraculus::LIGHT_CHANNEL + 11 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(15.38, 23.464), module, Oraculus::LIGHT_CHANNEL + 12 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.016, 20.267), module, Oraculus::LIGHT_CHANNEL + 13 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.827, 17.557), module, Oraculus::LIGHT_CHANNEL + 14 * 3));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.537, 15.746), module, Oraculus::LIGHT_CHANNEL + 15 * 3));
	}
};

Model* modelOraculus = createModel<Oraculus, OraculusWidget>("Sanguine-Monsters-Oraculus");