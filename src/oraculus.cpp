#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#ifndef METAMODULE
#include "seqcomponents.hpp"
#endif
#include "sanguinehelpers.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "pcg_random.hpp"
#pragma GCC diagnostic pop

struct Oraculus : SanguineModule {

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
#ifdef METAMODULE
		LIGHT_NO_REPEATS,
#endif
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;

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

	const static int kLightsFrequency = 16;

	pcg32 pcgRng;

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

		pcgRng = pcg32(static_cast<int>(std::round(system::getUnixTime())));

		lightsDivider.setDivision(kLightsFrequency);
	};

	void process(const ProcessArgs& args) override {
		channelCount = inputs[INPUT_POLYPHONIC].getChannels();

		if (lightsDivider.process()) {
			updateLights(args);
		}

		if (bResetConnected && stInputReset.process(inputs[INPUT_RESET].getVoltage())) {
			doResetTrigger();
		}

		if (btReset.process(params[PARAM_RESET].getValue())) {
			doResetTrigger();
		}

		finalChannel = -1;
		if (channelCount > 0) {
			if (selectedChannel >= channelCount) {
				selectedChannel = channelCount - 1;
			}

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

			finalChannel = selectedChannel;
			if (bCvConnected && channelCount > 1) {
				float cv = inputs[INPUT_CV_OFFSET].getVoltage();
				int channelOffset = std::floor(cv * (channelCount / 10.f));
				finalChannel = selectedChannel + channelOffset;
				if (finalChannel < 0) {
					finalChannel = channelCount + channelOffset;
				}
				if (finalChannel >= channelCount) {
					finalChannel = channelOffset - (channelCount - selectedChannel);
				}
			}

			if (bOutputConnected) {
				outputs[OUTPUT_MONOPHONIC].setVoltage(inputs[INPUT_POLYPHONIC].getVoltage(finalChannel));
			}
		} else {
			if (bOutputConnected) {
				outputs[OUTPUT_MONOPHONIC].setChannels(0);
			}
		}

		bNoRepeats = params[PARAM_NO_REPEATS].getValue();
	}

	void doDecreaseTrigger() {
		selectedChannel = ((selectedChannel - 1) + channelCount) % channelCount;
	};

	void doIncreaseTrigger() {
		selectedChannel = (selectedChannel + 1) % channelCount;
	};

	void doRandomTrigger() {
		if (!bNoRepeats) {
			selectedChannel = pcgRng(channelCount);
		} else {
			int randomNum = selectedChannel;
			while (randomNum == selectedChannel)
				randomNum = pcgRng(channelCount);
			selectedChannel = randomNum;
		}
	};

	void doResetTrigger() {
		selectedChannel = 0;
	};

	void updateLights(const ProcessArgs& args) {
		// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
		const float sampleTime = args.sampleTime * kLightsFrequency;

		for (int light = 0; light < PORT_MAX_CHANNELS; ++light) {
			int currentLight = light * 3;
			if (light == finalChannel) {
				lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.34f, sampleTime);
			} else if (light < channelCount) {
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.18f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.10f, sampleTime);
			} else {
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
		}

#ifdef METAMODULE
		lights[LIGHT_NO_REPEATS].setBrightness(static_cast<bool>(params[PARAM_NO_REPEATS].getValue()) ?
			kSanguineButtonLightValue : 0.f);
#endif
	}

	void onPortChange(const PortChangeEvent& e) override {
		switch (e.type) {
		case Port::INPUT:
			switch (e.portId) {
			case INPUT_CV_OFFSET:
				bCvConnected = e.connecting;
				break;

			case INPUT_INCREASE:
				bIncreaseConnected = e.connecting;
				break;

			case INPUT_DECREASE:
				bDecreaseConnected = e.connecting;
				break;

			case INPUT_RANDOM:
				bRandomConnected = e.connecting;
				break;

			case INPUT_RESET:
				bResetConnected = e.connecting;
				break;

			default:
				break;
			}
			break;

		case Port::OUTPUT:
			bOutputConnected = e.connecting;
			break;
		}
	}
};

struct OraculusWidget : SanguineModuleWidget {
	explicit OraculusWidget(Oraculus* module) {
		setModule(module);

		moduleName = "oraculus";
		panelSize = SIZE_7;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.452, 23.464), module, Oraculus::INPUT_POLYPHONIC));

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(22.734, 43.189), module, Oraculus::INPUT_CV_OFFSET));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 55.801), module, Oraculus::INPUT_INCREASE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 68.984), module, Oraculus::INPUT_DECREASE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 82.168), module, Oraculus::INPUT_RANDOM));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.452, 95.351), module, Oraculus::INPUT_RESET));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.78, 113.488), module, Oraculus::OUTPUT_MONOPHONIC));

#ifndef METAMODULE
		addParam(createParamCentered<SeqButtonUp>(millimetersToPixelsVec(25.451, 55.801), module, Oraculus::PARAM_INCREASE));

		addParam(createParamCentered<SeqButtonDown>(millimetersToPixelsVec(25.451f, 68.984f), module, Oraculus::PARAM_DECREASE));

		addParam(createParamCentered<SeqButtonRandom>(millimetersToPixelsVec(25.451, 82.168), module, Oraculus::PARAM_RANDOM));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(25.451, 95.351), module, Oraculus::PARAM_RESET));

		SanguinePolyInputLight* InPolyLight = new SanguinePolyInputLight(module, 6.452, 14.785);
		addChild(InPolyLight);

		SanguineMonoOutputLight* outMonoLight = new SanguineMonoOutputLight(module, 17.78, 106.565);
		addChild(outMonoLight);

		addParam(createParam<SeqButtonNoRepeatsSmall>(millimetersToPixelsVec(9.454, 41.189), module, Oraculus::PARAM_NO_REPEATS));
#else
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.188f, 55.801f), module, Oraculus::PARAM_INCREASE));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.188f, 68.984f), module, Oraculus::PARAM_DECREASE));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.188f, 82.168f), module, Oraculus::PARAM_RANDOM));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(28.188f, 95.351f), module, Oraculus::PARAM_RESET));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<PurpleLight>>>(millimetersToPixelsVec(11.454, 44.889), module,
			Oraculus::PARAM_NO_REPEATS, Oraculus::LIGHT_NO_REPEATS));
#endif

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