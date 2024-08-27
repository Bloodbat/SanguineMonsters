#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinedsp.hpp"
#include "alembic.hpp"

const float SaturatorFloat::limit = 12.f;

struct Alchemist : Module {

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

	bool channelMuted[PORT_MAX_CHANNELS] = {};
	bool channelSoloed[PORT_MAX_CHANNELS] = {};

	dsp::ClockDivider lightsDivider;
	dsp::VuMeter2 vuMeterMix;
	dsp::VuMeter2 vuMetersGain[PORT_MAX_CHANNELS];

	SaturatorFloat saturatorFloat;

	Alchemist() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			int channelNumber = i + 1;
			configParam(PARAM_GAIN + i, 0.f, 1.f, 0.f, string::f("Channel %d gain", channelNumber), "%", 0.f, 100.f);
			configButton(PARAM_MUTE + i, string::f("Channel %d mute", channelNumber));
			configButton(PARAM_SOLO + i, string::f("Channel %d solo", channelNumber));
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
		bool hasExpander = (alembicExpander && alembicExpander->getModel() == modelAlembic);

		if (bIsLightsTurn) {
			soloCount = 0;
			for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
				channelMuted[i] = bool(params[PARAM_MUTE + i].getValue());
				if (i < channelCount) {
					channelSoloed[i] = bool(params[PARAM_SOLO + i].getValue());
					if (channelSoloed[i]) {
						++soloCount;
					}
				}
				else {
					channelSoloed[i] = false;
				}
				if (params[PARAM_SOLO + i].getValue()) {
					if (channelMuted[i]) {
						channelMuted[i] = false;
						params[PARAM_MUTE + i].setValue(0.f);
					}
				}
			}
		}

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			if (i < channelCount) {
				outVoltages[i] = inputs[INPUT_POLYPHONIC].getPolyVoltage(i);

				if (hasExpander) {
					outVoltages[i] = outVoltages[i] * clamp(params[PARAM_GAIN + i].getValue() +
						alembicExpander->getInput(Alembic::INPUT_GAIN_CV + i).getVoltage() / 5.f, 0.f, 2.f);
				}
				else {
					outVoltages[i] = outVoltages[i] * params[PARAM_GAIN + i].getValue();
				}

				if (std::fabs(outVoltages[i]) >= 10.1f) {
					outVoltages[i] = saturatorFloat.next(outVoltages[i]);
				}

				if (!channelMuted[i] && (soloCount == 0 || channelSoloed[i])) {
					monoMix += outVoltages[i];
					masterOutVoltages[i] = outVoltages[i] * mixModulation;
				}
				else {
					masterOutVoltages[i] = 0.f;
				}
				if (hasExpander) {
					if (alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + i).isConnected()) {
						alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + i).setVoltage(masterOutVoltages[i]);
					}
				}
			}
			else {
				outVoltages[i] = 0.f;
				if (hasExpander) {
					if (alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + i).isConnected()) {
						alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + i).setVoltage(0.f);
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

			for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
				vuMetersGain[i].process(sampleTime, outVoltages[i] / 10.f);

				int currentLight = LIGHT_GAIN + i * 2;
				float redValue = vuMetersGain[i].getBrightness(0.f, 0.f);
				float yellowValue = vuMetersGain[i].getBrightness(-3.f, -1.f);
				float greenValue = vuMetersGain[i].getBrightness(-38.f, -1.f);
				bool isRed = redValue > 0;

				if (isRed) {
					lights[currentLight + 0].setBrightness(0.f);
					lights[currentLight + 1].setBrightness(redValue);
				}
				else {
					lights[currentLight + 0].setBrightness(greenValue);
					lights[currentLight + 1].setBrightness(yellowValue);
				}

				lights[LIGHT_MUTE + i].setBrightnessSmooth(params[PARAM_MUTE + i].getValue(), sampleTime);
				lights[LIGHT_SOLO + i].setBrightnessSmooth(params[PARAM_SOLO + i].getValue(), sampleTime);
			}
			vuMeterMix.process(sampleTime, monoMix / 10);
			lights[LIGHT_VU].setBrightness(vuMeterMix.getBrightness(-38.f, -19.f));
			lights[LIGHT_VU + 1].setBrightness(vuMeterMix.getBrightness(-19.f, -3.f));
			lights[LIGHT_VU + 2].setBrightness(vuMeterMix.getBrightness(-3.f, -1.f));
			lights[LIGHT_VU + 3].setBrightness(vuMeterMix.getBrightness(0.f, 0.f));

			lights[LIGHT_EXPANDER].setBrightnessSmooth(hasExpander, sampleTime);
		}
	}
};

struct AlchemistWidget : ModuleWidget {
	AlchemistWidget(Alchemist* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_23hp_purple.svg", "res/alchemist.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<MediumLight<OrangeLight>>(millimetersToPixelsVec(111.871, 9.672), module, Alchemist::LIGHT_EXPANDER));

		const float sliderBaseX = 8.329;
		const float deltaX = 14.307;

		float currentSliderX = sliderBaseX;

		for (int i = 0; i < 8; i++) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 28.145),
				module, Alchemist::PARAM_GAIN + i, Alchemist::LIGHT_GAIN + i * 2));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(currentSliderX, 46.911),
				module, Alchemist::PARAM_MUTE + i, Alchemist::LIGHT_MUTE + i));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(currentSliderX, 54.549),
				module, Alchemist::PARAM_SOLO + i, Alchemist::LIGHT_SOLO + i));
			currentSliderX += deltaX;
		}

		const int offset = 8;

		currentSliderX = sliderBaseX;

		for (int i = 0; i < 8; i++) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 73.478),
				module, Alchemist::PARAM_GAIN + i + offset, (Alchemist::LIGHT_GAIN + i + offset) * 2));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(currentSliderX, 92.218),
				module, Alchemist::PARAM_MUTE + i + offset, Alchemist::LIGHT_MUTE + i + offset));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(currentSliderX, 99.855),
				module, Alchemist::PARAM_SOLO + i + offset, Alchemist::LIGHT_SOLO + i + offset));
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

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.876, 116.769), module, Alchemist::INPUT_POLYPHONIC));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(95.442, 116.769), module, Alchemist::OUTPUT_POLYPHONIC_MIX));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(108.963, 116.769), module, Alchemist::OUTPUT_MONO_MIX));
	}
};

Model* modelAlchemist = createModel<Alchemist, AlchemistWidget>("Sanguine-Alchemist");