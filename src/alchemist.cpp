#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinedsp.hpp"
#include "alembic.hpp"
#include "alchemist.hpp"

struct Alchemist : SanguineModule {

	enum ParamIds {
		ENUMS(PARAM_GAIN, PORT_MAX_CHANNELS),
		PARAM_MIX,
		ENUMS(PARAM_MUTE, PORT_MAX_CHANNELS),
		ENUMS(PARAM_SOLO, PORT_MAX_CHANNELS),
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_POLYPHONIC,
		INPUT_MIX_CV,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_MONO_MIX,
		OUTPUT_POLYPHONIC_MIX,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_GAIN, 2 * PORT_MAX_CHANNELS),
		ENUMS(LIGHT_MUTE, PORT_MAX_CHANNELS),
		ENUMS(LIGHT_SOLO, PORT_MAX_CHANNELS),
		ENUMS(LIGHT_VU, 4),
		LIGHT_EXPANDER,
		LIGHTS_COUNT
	};

	const int kLightFrequency = 512;

	int soloCount = 0;

	bool bChannelMuted[PORT_MAX_CHANNELS] = {};
	bool bChannelSoloed[PORT_MAX_CHANNELS] = {};

	dsp::ClockDivider lightsDivider;
	dsp::VuMeter2 vuMeterMix;
	dsp::VuMeter2 vuMetersGain[PORT_MAX_CHANNELS];

	SaturatorFloat saturatorFloat;

	Alchemist() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			int channelNumber = channel + 1;
			configParam(PARAM_GAIN + channel, 0.f, 1.f, 0.f, string::f("Channel %d gain", channelNumber), "%", 0.f, 100.f);
			configButton(PARAM_MUTE + channel, string::f("Channel %d mute", channelNumber));
			configButton(PARAM_SOLO + channel, string::f("Channel %d solo", channelNumber));
		}

		configParam(PARAM_MIX, 0.f, 1.f, 1.f, "Master mix", "%", 0.f, 100.f);
		configInput(INPUT_MIX_CV, "Master mix CV");

		configInput(INPUT_POLYPHONIC, "Polyphonic");

		configOutput(OUTPUT_POLYPHONIC_MIX, "Polyphonic mix");
		configOutput(OUTPUT_MONO_MIX, "Monophonic mix");

		configBypass(INPUT_POLYPHONIC, OUTPUT_POLYPHONIC_MIX);

		lightsDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		float monoMix = 0.f;
		float outVoltages[PORT_MAX_CHANNELS] = {};
		float masterOutVoltages[PORT_MAX_CHANNELS] = {};
		float mixModulation = clamp(params[PARAM_MIX].getValue() + inputs[INPUT_MIX_CV].getVoltage() / 5.f, 0.f, 2.f);

		Module* alembicExpander = getRightExpander().module;

		int channelCount = inputs[INPUT_POLYPHONIC].getChannels();

		bool bIsLightsTurn = lightsDivider.process();
		bool bHasExpander = (alembicExpander && alembicExpander->getModel() == modelAlembic);

		if (bIsLightsTurn) {
			soloCount = 0;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				bChannelMuted[channel] = static_cast<bool>(params[PARAM_MUTE + channel].getValue());
				if (channel < channelCount) {
					bChannelSoloed[channel] = static_cast<bool>(params[PARAM_SOLO + channel].getValue());
					if (bChannelSoloed[channel]) {
						++soloCount;
					}
				} else {
					bChannelSoloed[channel] = false;
				}
				if (params[PARAM_SOLO + channel].getValue()) {
					if (bChannelMuted[channel]) {
						bChannelMuted[channel] = false;
						params[PARAM_MUTE + channel].setValue(0.f);
					}
				}
			}
		}

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			if (channel < channelCount) {
				outVoltages[channel] = inputs[INPUT_POLYPHONIC].getPolyVoltage(channel);

				if (bHasExpander) {
					outVoltages[channel] = outVoltages[channel] * clamp(params[PARAM_GAIN + channel].getValue() +
						alembicExpander->getInput(Alembic::INPUT_GAIN_CV + channel).getVoltage() / 5.f, 0.f, 2.f);
				} else {
					outVoltages[channel] = outVoltages[channel] * params[PARAM_GAIN + channel].getValue();
				}

				if (std::fabs(outVoltages[channel]) >= 10.1f) {
					outVoltages[channel] = saturatorFloat.next(outVoltages[channel]);
				}

				if (!bChannelMuted[channel] && (soloCount == 0 || bChannelSoloed[channel])) {
					monoMix += outVoltages[channel];
					masterOutVoltages[channel] = outVoltages[channel] * mixModulation;
				} else {
					masterOutVoltages[channel] = 0.f;
				}
				if (bHasExpander) {
					if (alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel).isConnected()) {
						alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel).setVoltage(masterOutVoltages[channel]);
					}
				}
			} else {
				outVoltages[channel] = 0.f;
				if (bHasExpander) {
					if (alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel).isConnected()) {
						alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel).setVoltage(0.f);
					}
				}
			}
		}

		monoMix = monoMix * mixModulation;

		if (std::fabs(monoMix) >= 10.1f) {
			monoMix = saturatorFloat.next(monoMix);
		}

		if (outputs[OUTPUT_MONO_MIX].isConnected()) {
			outputs[OUTPUT_MONO_MIX].setVoltage(monoMix);
		}

		if (outputs[OUTPUT_POLYPHONIC_MIX].isConnected()) {
			outputs[OUTPUT_POLYPHONIC_MIX].setChannels(channelCount);
			outputs[OUTPUT_POLYPHONIC_MIX].writeVoltages(masterOutVoltages);
		}

		if (bIsLightsTurn) {
			const float sampleTime = kLightFrequency * args.sampleTime;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				vuMetersGain[channel].process(sampleTime, outVoltages[channel] / 10.f);

				int currentLight = LIGHT_GAIN + channel * 2;
				float redValue = vuMetersGain[channel].getBrightness(0.f, 0.f);
				float yellowValue = vuMetersGain[channel].getBrightness(-3.f, -1.f);
				float greenValue = vuMetersGain[channel].getBrightness(-38.f, -1.f);
				bool bLightIsRed = redValue > 0;

				if (bLightIsRed) {
					lights[currentLight + 0].setBrightness(0.f);
					lights[currentLight + 1].setBrightness(redValue);
				} else {
					lights[currentLight + 0].setBrightness(greenValue);
					lights[currentLight + 1].setBrightness(yellowValue);
				}

				lights[LIGHT_MUTE + channel].setBrightnessSmooth(params[PARAM_MUTE + channel].getValue() ? 0.75f : 0.f, sampleTime);
				lights[LIGHT_SOLO + channel].setBrightnessSmooth(params[PARAM_SOLO + channel].getValue() ? 0.75f : 0.f, sampleTime);
			}
			vuMeterMix.process(sampleTime, monoMix / 10);
			lights[LIGHT_VU].setBrightness(vuMeterMix.getBrightness(-38.f, -19.f));
			lights[LIGHT_VU + 1].setBrightness(vuMeterMix.getBrightness(-19.f, -3.f));
			lights[LIGHT_VU + 2].setBrightness(vuMeterMix.getBrightness(-3.f, -1.f));
			lights[LIGHT_VU + 3].setBrightness(vuMeterMix.getBrightness(0.f, 0.f));

			lights[LIGHT_EXPANDER].setBrightnessSmooth(bHasExpander, sampleTime);
		}
	}
};

struct AlchemistWidget : SanguineModuleWidget {
	AlchemistWidget(Alchemist* module) {
		setModule(module);

		moduleName = "alchemist";
		panelSize = SIZE_23;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createLightCentered<MediumLight<OrangeLight>>(millimetersToPixelsVec(111.871, 9.672), module, Alchemist::LIGHT_EXPANDER));

		const float sliderBaseX = 8.329;
		const float deltaX = 14.307;

		float currentSliderX = sliderBaseX;

		for (int component = 0; component < 8; ++component) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 28.145),
				module, Alchemist::PARAM_GAIN + component, Alchemist::LIGHT_GAIN + component * 2));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(currentSliderX, 46.911),
				module, Alchemist::PARAM_MUTE + component, Alchemist::LIGHT_MUTE + component));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(currentSliderX, 54.549),
				module, Alchemist::PARAM_SOLO + component, Alchemist::LIGHT_SOLO + component));
			currentSliderX += deltaX;
		}

		static const int offset = 8;

		currentSliderX = sliderBaseX;

		for (int component = 0; component < 8; ++component) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 73.478),
				module, Alchemist::PARAM_GAIN + component + offset, (Alchemist::LIGHT_GAIN + component + offset) * 2));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(currentSliderX, 92.218),
				module, Alchemist::PARAM_MUTE + component + offset, Alchemist::LIGHT_MUTE + component + offset));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(currentSliderX, 99.855),
				module, Alchemist::PARAM_SOLO + component + offset, Alchemist::LIGHT_SOLO + component + offset));
			currentSliderX += deltaX;
		}

		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 7.876, 108.973);
		addChild(inLight);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 22.43, 110.65);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 35.563, 117.606);
		addChild(monstersLight);

		SanguineStaticRGBLight* sumLight = new SanguineStaticRGBLight(module, "res/sum_light_on.svg", 62.155, 108.973, true, kSanguineBlueLight);
		addChild(sumLight);

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(53.657, 114.094), module, Alchemist::INPUT_MIX_CV));

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(72.754, 114.094), module, Alchemist::PARAM_MIX));

		SanguinePolyOutputLight* outPolyLight = new SanguinePolyOutputLight(module, 95.442, 108.973);
		addChild(outPolyLight);

		SanguineMonoOutputLight* outMonoLight = new SanguineMonoOutputLight(module, 108.963, 108.973);
		addChild(outMonoLight);

		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(84.329, 120.011), module, Alchemist::LIGHT_VU));
		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(84.329, 115.955), module, Alchemist::LIGHT_VU + 1));
		addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(84.329, 111.878), module, Alchemist::LIGHT_VU + 2));
		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(84.329, 107.776), module, Alchemist::LIGHT_VU + 3));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.876, 116.769), module, Alchemist::INPUT_POLYPHONIC));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(95.442, 116.769), module, Alchemist::OUTPUT_POLYPHONIC_MIX));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(108.963, 116.769), module, Alchemist::OUTPUT_MONO_MIX));
	}
};

Model* modelAlchemist = createModel<Alchemist, AlchemistWidget>("Sanguine-Alchemist");